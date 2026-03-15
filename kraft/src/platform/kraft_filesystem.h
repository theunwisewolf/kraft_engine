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

struct FileSystemEntry
{
    String8 name;
    union
    {
        u64  file_size;
        bool is_file;
    };
};

struct Directory
{
    FileSystemEntry* entries;
    u32              entry_count;
};

struct FileHandle
{
    FILE* Handle;
};

struct FileMMapHandle
{
    void* Handle;
    u64   Size;
};

enum FileOpenMode
{
    FILE_OPEN_MODE_READ = 0x1,
    FILE_OPEN_MODE_WRITE = 0x2,
    FILE_OPEN_MODE_APPEND = 0x4,
};

// File helpers
KRAFT_API bool OpenFile(String8 path, int mode, bool binary, FileHandle* out);
KRAFT_API u64  GetFileSize(FileHandle* handle);
KRAFT_API void CloseFile(FileHandle* handle);

// Returns true if the file exists at "path"
KRAFT_API bool FileExists(String8 path);

// Very basic for now, just replaces all windows path separators
// with unix path separators
KRAFT_API String8 CleanPath(ArenaAllocator* arena, String8 path);

// Returns the directory without the filename
KRAFT_API String8 Dirname(ArenaAllocator* arena, String8 path);

// Returns the filename without the directory
KRAFT_API String8 Basename(ArenaAllocator* arena, String8 path);

KRAFT_API String8 PathJoin(ArenaAllocator* arena, String8 path1, String8 path2);

// Converts a relative path to an absolute path
KRAFT_API String8 AbsolutePath(ArenaAllocator* arena, String8 path);

// outBuffer, if null is allocated & must be freed by the caller
KRAFT_API bool   ReadAllBytes(FileHandle* handle, u8** out_buffer, u64* bytes_read = nullptr);
KRAFT_API buffer ReadAllBytes(ArenaAllocator* arena, FileHandle* handle);
KRAFT_API buffer ReadAllBytes(ArenaAllocator* arena, String8 path);
KRAFT_API bool   WriteFile(FileHandle* handle, const u8* buffer, u64 size);

// Returns the last modified time of the file in seconds since epoch
// Returns 0 if the file doesn't exist or an error occurs
KRAFT_API u64 GetFileModifiedTime(String8 path);

// Platform dependent APIs
KRAFT_API u32            GetFileCount(String8 path);
KRAFT_API Directory      ReadDir(ArenaAllocator* arena, String8 path);
KRAFT_API FileMMapHandle MMap(const String& Path);

//
// File watcher
//

enum FileWatchEventType
{
    FILE_WATCH_EVENT_MODIFIED,
    FILE_WATCH_EVENT_CREATED,
    FILE_WATCH_EVENT_DELETED,
    FILE_WATCH_EVENT_RENAMED,
};

struct FileWatchEvent
{
    FileWatchEventType type;
    String8            file_path;
    String8            directory;
};

typedef void (*FileWatchCallback)(FileWatchEvent event, void* userdata);

struct FileWatchEntry
{
    String8           directory;
    FileWatchCallback callback;
    void*             userdata;
    bool              recursive;
    void*             platform_data;
};

#define FILE_WATCHER_MAX_WATCHES 16

struct FileWatcherState
{
    ArenaAllocator* arena;
    FileWatchEntry  watches[FILE_WATCHER_MAX_WATCHES];
    u32             watch_count;
};

struct KRAFT_API FileWatcher
{
    static bool Init(ArenaAllocator* arena);
    static void Shutdown();

    // Start watching a directory for changes
    // Returns the watch index, or -1 on failure
    static i32 WatchDirectory(String8 directory, bool recursive, FileWatchCallback callback, void* userdata);

    // Stop watching a directory
    static void UnwatchDirectory(i32 watch_index);

    // Poll for changes and dispatch callbacks
    static void ProcessChanges();
};

} // namespace fs

}; // namespace kraft