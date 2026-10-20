#include <platform/kraft_filesystem.h>
#include <platform/kraft_filesystem_internal.h>
#include "kraft_macos_cocoa.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/attr.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <unistd.h>

#include <core/kraft_core_includes.h>

namespace kraft::fs {

// The path with a zero after it, which the system calls want; false if it does not fit
static bool MacPath(char* out, u64 capacity, String8 path) {
    if (path.count >= capacity) {
        return false;
    }

    MemCpy(out, path.ptr, path.count);
    out[path.count] = 0;

    return true;
}

KRAFT_API bool FileExists(String8 path) {
    char native[PATH_MAX];
    struct stat info;

    return MacPath(native, sizeof(native), path) && stat(native, &info) == 0 && !S_ISDIR(info.st_mode);
}

KRAFT_API bool DirectoryExists(String8 path) {
    char native[PATH_MAX];
    struct stat info;

    return MacPath(native, sizeof(native), path) && stat(native, &info) == 0 && S_ISDIR(info.st_mode);
}

KRAFT_API bool MakeDirectory(String8 path) {
    char native[PATH_MAX];
    if (!MacPath(native, sizeof(native), path)) {
        return false;
    }

    return mkdir(native, 0755) == 0 || errno == EEXIST;
}

KRAFT_API bool MakeDirectories(ArenaAllocator* arena, String8 path) {
    TempArena scratch = ScratchBegin(&arena, 1);
    String8 cleaned = CleanPath(scratch.arena, path);
    bool ok = true;

    for (u64 index = 1; index < cleaned.count && ok; index++) {
        if (cleaned.str[index] != '/') {
            continue;
        }

        ok = MakeDirectory(String8FromPtrAndLength(cleaned.ptr, index));
    }

    if (ok) {
        ok = MakeDirectory(cleaned);
    }
    ScratchEnd(scratch);

    return ok;
}

KRAFT_API bool RemoveFile(String8 path) {
    char native[PATH_MAX];
    if (!MacPath(native, sizeof(native), path)) {
        return false;
    }

    return unlink(native) == 0 || errno == ENOENT || errno == ENOTDIR;
}

KRAFT_API bool RemoveEmptyDirectory(String8 path) {
    char native[PATH_MAX];
    if (!MacPath(native, sizeof(native), path)) {
        return false;
    }

    if (rmdir(native) == 0 || errno == ENOENT) {
        return true;
    }

    // ENOTDIR also says the path is a file or a link, which is not gone
    struct stat info;
    return errno == ENOTDIR && lstat(native, &info) != 0;
}

KRAFT_API bool MovePath(String8 from, String8 to, bool replace_existing) {
    char native_from[PATH_MAX];
    char native_to[PATH_MAX];
    if (!MacPath(native_from, sizeof(native_from), from) || !MacPath(native_to, sizeof(native_to), to)) {
        return false;
    }

    // rename replaces what is at `to`; RENAME_EXCL fails instead, as Windows does without being asked to replace
    if (replace_existing) {
        return rename(native_from, native_to) == 0;
    }

    return renamex_np(native_from, native_to, RENAME_EXCL) == 0;
}

KRAFT_API bool MoveToTrash(String8 path) {
    bool trashed = false;
    @autoreleasepool {
        NSString* native = [[[NSString alloc] initWithBytes:path.ptr length:path.count encoding:NSUTF8StringEncoding] autorelease];
        if (native) {
            trashed = [[NSFileManager defaultManager] trashItemAtURL:[NSURL fileURLWithPath:native] resultingItemURL:nil error:nil];
        }
    }

    return trashed;
}

KRAFT_API FileReadStatus ReadEntireFile(ArenaAllocator* arena, String8 path, String8* out_contents) {
    *out_contents = String8{};
    char native[PATH_MAX];
    if (!MacPath(native, sizeof(native), path)) {
        return FILE_READ_STATUS_MISSING;
    }

    int file = open(native, O_RDONLY | O_CLOEXEC);
    if (file < 0) {
        return errno == ENOENT || errno == ENOTDIR ? FILE_READ_STATUS_MISSING : FILE_READ_STATUS_BUSY;
    }

    struct stat info;
    if (fstat(file, &info) != 0 || S_ISDIR(info.st_mode) || info.st_size < 0 || info.st_size > 0x7FFFFFFF) {
        close(file);
        return FILE_READ_STATUS_BUSY;
    }

    u64 capacity = (u64)info.st_size;
    u8* bytes = ArenaPushArrayNoZero(arena, u8, capacity + 1);
    u64 total = 0;
    while (total < capacity) {
        ssize_t got = read(file, bytes + total, capacity - total);
        if (got < 0 && errno == EINTR) {
            continue;
        }

        if (got <= 0) {
            break;
        }
        total += (u64)got;
    }
    close(file);
    bytes[total] = 0;
    *out_contents = String8FromPtrAndLength(bytes, total);

    return FILE_READ_STATUS_OK;
}

//
// Directory listing
//

// What getattrlistbulk says about one entry; `name` points into its buffer and is only good during the visit
struct MacListingEntry {
    const char* name;
    u64 name_length;
    bool is_directory;
    u64 size;
    u64 modified_time;
};

typedef void MacListingVisit(MacListingEntry* entry, void* user);

// Walks a directory with getattrlistbulk, which hands back the name, type, size and time of many entries per
// call, rather than taking a stat per file
static bool MacListDirectory(const char* directory, MacListingVisit* visit, void* user) {
    int directory_file = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory_file < 0) {
        return false;
    }

    struct attrlist attributes = {};
    attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
    attributes.commonattr = ATTR_CMN_RETURNED_ATTRS | ATTR_CMN_NAME | ATTR_CMN_ERROR | ATTR_CMN_OBJTYPE | ATTR_CMN_MODTIME;
    attributes.fileattr = ATTR_FILE_DATALENGTH;

    u8 buffer[KRAFT_SIZE_KB(32)];
    for (;;) {
        int count = getattrlistbulk(directory_file, &attributes, buffer, sizeof(buffer), 0);
        if (count <= 0) {
            break;
        }

        // The fields of an entry follow each other in the order of their attribute bits, the error first
        u8* entry_start = buffer;
        for (int index = 0; index < count; index++) {
            u8* field = entry_start;
            u32 length = 0;
            MemCpy(&length, field, sizeof(length));
            entry_start += length;
            field += sizeof(u32);

            attribute_set_t returned = {};
            MemCpy(&returned, field, sizeof(returned));
            field += sizeof(attribute_set_t);

            u32 error = 0;
            if (returned.commonattr & ATTR_CMN_ERROR) {
                MemCpy(&error, field, sizeof(error));
                field += sizeof(u32);
            }

            MacListingEntry entry = {};
            if (returned.commonattr & ATTR_CMN_NAME) {
                attrreference_t name_reference = {};
                MemCpy(&name_reference, field, sizeof(name_reference));
                entry.name = (const char*)field + name_reference.attr_dataoffset;
                entry.name_length = name_reference.attr_length > 0 ? name_reference.attr_length - 1 : 0;
                field += sizeof(attrreference_t);
            }

            if (error != 0 || !entry.name) {
                continue;
            }

            fsobj_type_t type = VNON;
            if (returned.commonattr & ATTR_CMN_OBJTYPE) {
                MemCpy(&type, field, sizeof(type));
                field += sizeof(fsobj_type_t);
            }

            if (returned.commonattr & ATTR_CMN_MODTIME) {
                struct timespec modified = {};
                MemCpy(&modified, field, sizeof(modified));
                field += sizeof(struct timespec);
                entry.modified_time = modified.tv_sec > 0 ? (u64)modified.tv_sec : 0;
            }

            if (returned.fileattr & ATTR_FILE_DATALENGTH) {
                off_t size = 0;
                MemCpy(&size, field, sizeof(size));
                field += sizeof(off_t);
                entry.size = size > 0 ? (u64)size : 0;
            }

            entry.is_directory = type == VDIR;

            // A link is never a folder to go into, so nothing that walks a tree, deleting it least of all, reaches
            // through one into what it points at. One to a file reads as that file.
            if (type == VLNK) {
                char target[PATH_MAX];
                struct stat info;
                snprintf(target, sizeof(target), "%s/%.*s", directory, (int)entry.name_length, entry.name);
                if (stat(target, &info) == 0 && S_ISREG(info.st_mode)) {
                    entry.size = (u64)info.st_size;
                    entry.modified_time = (u64)info.st_mtimespec.tv_sec;
                }
            }

            visit(&entry, user);
        }
    }

    close(directory_file);

    return true;
}

struct MacListingNode {
    FileSystemEntry entry;
    MacListingNode* next;
};

struct MacListing {
    ArenaAllocator* arena; // where the names go
    ArenaAllocator* nodes_arena;
    MacListingNode* first;
    MacListingNode* last;
    u32 count;
};

static void MacListingAdd(MacListingEntry* entry, void* user) {
    MacListing* listing = (MacListing*)user;
    MacListingNode* node = ArenaPush(listing->nodes_arena, MacListingNode);
    node->entry.name = ArenaPushString8Copy(listing->arena, String8FromPtrAndLength((u8*)entry->name, entry->name_length));
    node->entry.is_directory = entry->is_directory;
    node->entry.file_size = entry->is_directory ? 0 : entry->size;
    node->entry.modified_time = entry->modified_time;
    if (listing->last) {
        listing->last->next = node;
    } else {
        listing->first = node;
    }

    listing->last = node;
    listing->count++;
}

// The order NTFS lists a folder in, which callers have come to expect: by name with case folded to upper, so "a_b"
// follows "ab"; names that differ only in case, as they can on a case-sensitive volume, by their bytes
static int MacListingCompare(const void* left, const void* right) {
    String8 a = ((const FileSystemEntry*)left)->name;
    String8 b = ((const FileSystemEntry*)right)->name;
    u64 count = a.count < b.count ? a.count : b.count;
    for (u64 index = 0; index < count; index++) {
        u8 a_character = a.ptr[index] >= 'a' && a.ptr[index] <= 'z' ? a.ptr[index] - 32 : a.ptr[index];
        u8 b_character = b.ptr[index] >= 'a' && b.ptr[index] <= 'z' ? b.ptr[index] - 32 : b.ptr[index];
        if (a_character != b_character) {
            return a_character < b_character ? -1 : 1;
        }
    }

    if (a.count != b.count) {
        return a.count < b.count ? -1 : 1;
    }

    return MemCmp(a.ptr, b.ptr, a.count);
}

static void MacListingCountFile(MacListingEntry* entry, void* user) {
    if (!entry->is_directory) {
        (*(u32*)user)++;
    }
}

KRAFT_API Directory ReadDirEntries(ArenaAllocator* arena, String8 path) {
    Directory result = {};
    char native[PATH_MAX];
    if (!MacPath(native, sizeof(native), path)) {
        return result;
    }

    TempArena scratch = ScratchBegin(&arena, 1);
    MacListing listing = {};
    listing.arena = arena;
    listing.nodes_arena = scratch.arena;
    if (MacListDirectory(native, MacListingAdd, &listing) && listing.count > 0) {
        result.entries = ArenaPushArray(arena, FileSystemEntry, listing.count);
        for (MacListingNode* node = listing.first; node; node = node->next) {
            result.entries[result.entry_count++] = node->entry;
        }

        // APFS hands entries back in no particular order
        qsort(result.entries, result.entry_count, sizeof(FileSystemEntry), MacListingCompare);
    }
    ScratchEnd(scratch);

    return result;
}

KRAFT_API u32 GetFileCount(String8 path) {
    char native[PATH_MAX];
    u32 count = 0;
    if (MacPath(native, sizeof(native), path)) {
        MacListDirectory(native, MacListingCountFile, &count);
    }

    return count;
}

KRAFT_API Directory ReadDir(ArenaAllocator* arena, String8 path) {
    Directory listing = ReadDirEntries(arena, path);
    u32 file_count = 0;
    for (u32 index = 0; index < listing.entry_count; index++) {
        if (!listing.entries[index].is_directory) {
            listing.entries[file_count++] = listing.entries[index];
        }
    }

    listing.entry_count = file_count;

    return listing;
}

//
// Directory watch
//

struct MacDirectoryWatch {
    FSEventStreamRef stream;
    dispatch_queue_t queue;
    char root[PATH_MAX]; // the real path, without a slash at the end, as FSEvents reports paths
    u64 root_length;
};

static void MacDirectoryWatchCallback(ConstFSEventStreamRef stream, void* user, size_t event_count, void* event_paths, const FSEventStreamEventFlags* flags, const FSEventStreamEventId* ids) {
    DirectoryWatch* watch = (DirectoryWatch*)user;
    MacDirectoryWatch* platform = (MacDirectoryWatch*)watch->platform;
    const char** paths = (const char**)event_paths;

    MutexLock(&watch->lock);
    bool was_pending = DirectoryWatchHasPending(watch);
    u64 used_before = watch->queue_used;
    bool overflowed_before = watch->overflowed;
    bool lost_before = watch->lost;
    for (size_t index = 0; index < event_count; index++) {
        FSEventStreamEventFlags event = flags[index];
        if (event & (kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagUserDropped | kFSEventStreamEventFlagKernelDropped)) {
            watch->overflowed = true;
        }

        if (event & kFSEventStreamEventFlagRootChanged) {
            watch->lost = true;
            continue;
        }

        const char* path = paths[index];
        u64 length = strlen(path);
        if (length <= platform->root_length + 1 || MemCmp(path, platform->root, platform->root_length) != 0 || path[platform->root_length] != '/') {
            continue;
        }

        String8 relative_path = String8FromPtrAndLength((u8*)path + platform->root_length + 1, length - platform->root_length - 1);
        if (relative_path.count > 0 && relative_path.ptr[relative_path.count - 1] == '/') {
            relative_path.count--;
        }

        bool nested = false;
        for (u64 at = 0; at < relative_path.count && !nested; at++) {
            nested = relative_path.ptr[at] == '/';
        }

        if (relative_path.count == 0 || (nested && !watch->recursive)) {
            continue;
        }

        if (!watch->ignore || !watch->ignore(relative_path)) {
            DirectoryWatchPush(watch, relative_path);
        }
    }

    bool arrived = watch->queue_used != used_before || watch->overflowed != overflowed_before || watch->lost != lost_before;
    if (arrived) {
        DirectoryWatchStamp(watch, was_pending);
    }
    MutexUnlock(&watch->lock);

    if (arrived && watch->wake) {
        watch->wake();
    }
}

KRAFT_API DirectoryWatch* DirectoryWatchStart(String8 path, bool recursive, DirectoryWatchIgnore* ignore, DirectoryWatchWake* wake) {
    // FSEvents names what changed by its real path: links resolved and every name as it is on disk
    char native[PATH_MAX];
    char real[PATH_MAX];
    int directory_file = MacPath(native, sizeof(native), path) ? open(native, O_RDONLY | O_DIRECTORY | O_CLOEXEC) : -1;
    bool resolved = directory_file >= 0 && fcntl(directory_file, F_GETPATH, real) != -1;
    if (directory_file >= 0) {
        close(directory_file);
    }

    if (!resolved) {
        KWARN("[DirectoryWatch]: cannot watch %S (errno %d)", path, errno);
        return nullptr;
    }

    DirectoryWatch* watch = DirectoryWatchCreate(recursive, ignore, wake);
    MacDirectoryWatch* platform = ArenaPush(watch->arena, MacDirectoryWatch);
    platform->root_length = strlen(real);
    while (platform->root_length > 1 && real[platform->root_length - 1] == '/') {
        platform->root_length--;
    }

    MemCpy(platform->root, real, platform->root_length);
    platform->root[platform->root_length] = 0;
    watch->platform = platform;

    FSEventStreamContext context = {};
    context.info = watch;
    CFStringRef root = CFStringCreateWithCString(kCFAllocatorDefault, platform->root, kCFStringEncodingUTF8);
    CFArrayRef roots = CFArrayCreate(kCFAllocatorDefault, (const void**)&root, 1, &kCFTypeArrayCallBacks);
    FSEventStreamCreateFlags flags = kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagWatchRoot;
    platform->stream = FSEventStreamCreate(kCFAllocatorDefault, MacDirectoryWatchCallback, &context, roots, kFSEventStreamEventIdSinceNow, 0.02, flags);
    CFRelease(roots);
    CFRelease(root);
    if (!platform->stream) {
        KWARN("[DirectoryWatch]: cannot watch %S", path);
        DirectoryWatchDestroy(watch);
        return nullptr;
    }

    platform->queue = dispatch_queue_create("kraft.directory-watch", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(platform->stream, platform->queue);
    if (!FSEventStreamStart(platform->stream)) {
        KWARN("[DirectoryWatch]: cannot start watching %S", path);
        FSEventStreamInvalidate(platform->stream);
        FSEventStreamRelease(platform->stream);
        dispatch_release(platform->queue);
        DirectoryWatchDestroy(watch);
        return nullptr;
    }

    return watch;
}

KRAFT_API void DirectoryWatchStop(DirectoryWatch* watch) {
    if (!watch) {
        return;
    }

    MacDirectoryWatch* platform = (MacDirectoryWatch*)watch->platform;
    FSEventStreamStop(platform->stream);
    FSEventStreamInvalidate(platform->stream);
    FSEventStreamRelease(platform->stream);

    // A callback already running finishes before the watch goes
    dispatch_sync(platform->queue, ^{});
    dispatch_release(platform->queue);
    DirectoryWatchDestroy(watch);
}

} // namespace kraft::fs
