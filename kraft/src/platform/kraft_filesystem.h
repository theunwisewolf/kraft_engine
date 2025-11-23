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

namespace filesystem {

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
KRAFT_API void    CleanPath(const char* path, char* out);
KRAFT_API String8 CleanPath(ArenaAllocator* arena, String8 path);

// Returns the directory without the filename
KRAFT_API void    Dirname(const char* path, char* out);
KRAFT_API void    Dirname(const char* Path, u64 PathLength, char* Out);
KRAFT_API String8 Dirname(ArenaAllocator* arena, String8 path);
KRAFT_API char*   Dirname(ArenaAllocator* arena, const char* Path);
KRAFT_API char*   Dirname(ArenaAllocator* arena, const char* Path, u64 PathLength);

// Returns the filename without the directory
KRAFT_API void    Basename(const char* path, char* out);
KRAFT_API String8 Basename(ArenaAllocator* arena, String8 path);

KRAFT_API String8 PathJoin(ArenaAllocator* arena, String8 path1, String8 path2);

// outBuffer, if null is allocated & must be freed by the caller
KRAFT_API bool   ReadAllBytes(FileHandle* handle, uint8** outBuffer, u64* bytesRead = nullptr);
KRAFT_API buffer ReadAllBytes(ArenaAllocator* arena, FileHandle* handle);
KRAFT_API buffer ReadAllBytes(ArenaAllocator* arena, String8 path);
KRAFT_API bool   WriteFile(FileHandle* Handle, const uint8* Buffer, u64 Size);
KRAFT_API bool   WriteFile(FileHandle* Handle, const kraft::Buffer& Buffer);

// Platform dependent APIs
KRAFT_API u32            GetFileCount(String8 path);
KRAFT_API Directory      ReadDir(ArenaAllocator* arena, String8 path);
KRAFT_API FileMMapHandle MMap(const String& Path);
} // namespace filesystem

}; // namespace kraft