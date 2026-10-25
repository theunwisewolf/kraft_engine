#pragma once

#include <core/kraft_core.h>

#ifdef KRAFT_PLATFORM_WINDOWS
#define KRAFT_PATH_SEPARATOR '\\'
#else
#define KRAFT_PATH_SEPARATOR '/'
#endif

namespace kraft {
struct Buffer;
struct ArenaAllocator;

namespace fs {

struct FileSystemEntry {
    String8 name;
    union {
        u64 file_size;
        bool is_file;
    };
    bool is_directory;
    bool is_link;
    u64 modified_time;
};

struct Directory {
    FileSystemEntry* entries;
    u32 entry_count;
};

struct FileHandle {
    FILE* Handle;
};

struct FileMMapHandle {
    void* Handle;
    u64 Size;
};

enum FileOpenMode {
    FILE_OPEN_MODE_READ = 0x1,
    FILE_OPEN_MODE_WRITE = 0x2,
    FILE_OPEN_MODE_APPEND = 0x4,
};

// File helpers
KRAFT_API bool OpenFile(String8 path, int mode, bool binary, FileHandle* out);
KRAFT_API u64 GetFileSize(FileHandle* handle);
KRAFT_API void CloseFile(FileHandle* handle);

// Returns true if the file exists at "path"
KRAFT_API bool FileExists(String8 path);

// Returns true if a file is at "path", with its size in bytes in out_size
KRAFT_API bool FileSize(String8 path, u64* out_size);

// Cleans a path similar to Go's path.Clean:
// - Replaces backslashes with forward slashes
// - Collapses multiple consecutive slashes into one
// - Eliminates . (current directory) elements
// - Eliminates inner .. elements along with the parent
// - Strips trailing slashes (unless the path is just "/")
// Returns "." for empty paths
KRAFT_API String8 CleanPath(ArenaAllocator* arena, String8 path);

// Returns the directory without the filename
KRAFT_API String8 Dirname(ArenaAllocator* arena, String8 path);

// Returns the filename without the directory
KRAFT_API String8 Basename(ArenaAllocator* arena, String8 path);

KRAFT_API String8 PathJoin(ArenaAllocator* arena, String8 path1, String8 path2);

// Converts a relative path to an absolute path
KRAFT_API String8 AbsolutePath(ArenaAllocator* arena, String8 path);

// [DEPRECATED]: Use the one with the arena_allocator
// out_buffer, if null is allocated and must be freed by the caller
KRAFT_API bool ReadAllBytes(FileHandle* handle, u8** out_buffer, u64* bytes_read = nullptr);

// Reads all data from the given file handle and returns a buffer allocated in the provided arena
KRAFT_API buffer ReadAllBytes(ArenaAllocator* arena, FileHandle* handle);

// Reads all data from the file at `path` and returns a buffer allocated in the provided arena
KRAFT_API buffer ReadAllBytes(ArenaAllocator* arena, String8 path);

// Writes the provided `buffer` of `size` to the file `handle`
KRAFT_API bool WriteFile(FileHandle* handle, const u8* buffer, u64 size);

// Returns the last modified time of the file in seconds since epoch
// Returns 0 if the file doesn't exist or an error occurs
KRAFT_API u64 GetFileModifiedTime(String8 path);

// Platform dependent APIs
KRAFT_API u32 GetFileCount(String8 path);
KRAFT_API Directory ReadDir(ArenaAllocator* arena, String8 path);

// Files and sub-directories (excluding "." and ".."), with is_directory set on each entry
KRAFT_API Directory ReadDirEntries(ArenaAllocator* arena, String8 path);
KRAFT_API bool DirectoryExists(String8 path);

// Creates one directory
// returns `true` if it was created or was already there
KRAFT_API bool MakeDirectory(String8 path);

// Creates every missing directory along the path, similar to mkdir -p
KRAFT_API bool MakeDirectories(ArenaAllocator* arena, String8 path);

// Deletes one file
// returns `true` if it is gone, including when it was never there
KRAFT_API bool RemoveFile(String8 path);

// Deletes one directory, which has to be empty already
KRAFT_API bool RemoveEmptyDirectory(String8 path);

// Deletes a file, or a directory and everything in it
// A link is deleted as a link, here or anywhere inside, and never what it points to
KRAFT_API bool RemoveTree(String8 path);

//
// Links
//

struct LinkInfo {
    String8 target; // as the link holds it, relative or absolute
    bool directory; // it points at a directory, which Windows makes a different kind of link for
    bool junction;  // a Windows junction rather than a symbolic link; its target is always absolute
};

// Returns true if `path` itself is a link, without following it
KRAFT_API bool IsLink(String8 path);

// Fills `out` with what the link at `path` holds; returns false if it is not a link
KRAFT_API bool ReadLink(ArenaAllocator* arena, String8 path, LinkInfo* out);

// Makes a link at `path`, as `link` describes
// Windows makes a symbolic link only with Developer Mode on, so one to a directory becomes a junction without it
KRAFT_API bool MakeLink(String8 path, const LinkInfo* link);

// Deletes the link at `path`, never what it points to
// returns `true` if it is gone, including when it was never there
KRAFT_API bool RemoveLink(String8 path);

// What a file or directory is, whichever path or link reaches it
struct FileIdentity {
    u64 volume;
    u64 index;
};

// Fills `out` with the identity of what `path` names, following links; returns false if there is nothing there
KRAFT_API bool GetFileIdentity(String8 path, FileIdentity* out);

// Moves the given path
// Fails if `to` exists, unless replace_existing is true
KRAFT_API bool MovePath(String8 from, String8 to, bool replace_existing = false);

// Moves the file or directory at `path` to the system's trash, from where the user can restore it
KRAFT_API bool MoveToTrash(String8 path);

enum FileReadStatus {
    FILE_READ_STATUS_OK,
    FILE_READ_STATUS_MISSING,
    FILE_READ_STATUS_BUSY, // locked by another program
};

// Reads a file in its entirety
// The output string is zero-terminated
KRAFT_API FileReadStatus ReadEntireFile(ArenaAllocator* arena, String8 path, String8* out_contents);

//
// Directory watch
//

// Watches a directory for changes on a background thread
// The paths of the files that changed are collected until DirectoryWatchTakeChanges is called
struct DirectoryWatch;

// Called on the watch thread for every changed path; return true to drop the path
// It runs without a thread context, so it can't log
typedef bool DirectoryWatchIgnore(String8 relative_path);

// Called on the watch thread when new changes arrive, to wake up a main loop that is waiting for events
// It runs without a thread context, so it can't log
typedef void DirectoryWatchWake();

struct DirectoryWatchChanges {
    String8* paths; // relative to the watched directory, with forward slashes; a path can appear more than once
    u32 path_count;
    bool overflowed; // more changes arrived than could be recorded, so `paths` is incomplete
    bool lost; // the directory was deleted or can no longer be read, and the watch has stopped
};

// Starts watching the directory at `path`, and every directory inside it if `recursive` is true
// `ignore` and `wake` are optional; returns null if the directory can't be watched
KRAFT_API DirectoryWatch* DirectoryWatchStart(String8 path, bool recursive, DirectoryWatchIgnore* ignore, DirectoryWatchWake* wake);

// Stops the watch and frees it
KRAFT_API void DirectoryWatchStop(DirectoryWatch* watch);

// Returns true if there are changes waiting to be taken
// `out_seconds_since_last` and `out_seconds_since_first` are the time since the newest and the oldest of them, so the
// caller can wait for a burst of changes (like a git pull) to settle before taking them
KRAFT_API bool DirectoryWatchHasChanges(DirectoryWatch* watch, f64* out_seconds_since_last, f64* out_seconds_since_first);

// Returns every change collected since the last call, copied into the provided arena, and clears them from the watch
KRAFT_API DirectoryWatchChanges DirectoryWatchTakeChanges(DirectoryWatch* watch, ArenaAllocator* arena);

// Puts paths back into the watch, so the next DirectoryWatchTakeChanges returns them again
// Useful for a file another program was still writing when it was read
KRAFT_API void DirectoryWatchPutBack(DirectoryWatch* watch, String8* paths, u32 path_count);

} // namespace fs

}; // namespace kraft