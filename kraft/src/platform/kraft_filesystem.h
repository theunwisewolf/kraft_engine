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

struct FileHandle;
struct FileInfo;
struct FileMMapHandle;

// File helpers
KRAFT_API bool   OpenFile(const char* path, int mode, bool binary, FileHandle* out);
KRAFT_API bool   OpenFile(const String& Path, int Mode, bool Binary, FileHandle* Out);
KRAFT_API uint64 GetFileSize(FileHandle* handle);
KRAFT_API void   CloseFile(FileHandle* handle);

// Returns true if the file exists at "path"
KRAFT_API bool FileExists(const char* path);
KRAFT_API bool FileExists(const String& path);

// Very basic for now, just replaces all windows path separators
// with unix path separators
KRAFT_API void    CleanPath(const char* path, char* out);
KRAFT_API String8 CleanPath(ArenaAllocator* arena, String8 path);

// Returns the directory without the filename
KRAFT_API void    Dirname(const char* path, char* out);
KRAFT_API void    Dirname(const char* Path, uint64 PathLength, char* Out);
KRAFT_API String8 Dirname(ArenaAllocator* arena, String8 path);
KRAFT_API char*   Dirname(ArenaAllocator* arena, const char* Path);
KRAFT_API char*   Dirname(ArenaAllocator* arena, const char* Path, uint64 PathLength);

// Returns the filename without the directory
KRAFT_API void    Basename(const char* path, char* out);
KRAFT_API String8 Basename(ArenaAllocator* arena, String8 path);

KRAFT_API char* PathJoin(ArenaAllocator* Arena, const char* A, const char* B);

// outBuffer, if null is allocated & must be freed by the caller
KRAFT_API bool   ReadAllBytes(FileHandle* handle, uint8** outBuffer, uint64* bytesRead = nullptr);
KRAFT_API buffer ReadAllBytes(ArenaAllocator* arena, FileHandle* handle);
KRAFT_API bool   ReadAllBytes(const char* path, uint8** outBuffer, uint64* bytesRead = nullptr);
KRAFT_API bool   WriteFile(FileHandle* Handle, const uint8* Buffer, uint64 Size);
KRAFT_API bool   WriteFile(FileHandle* Handle, const kraft::Buffer& Buffer);

// Platform dependent APIs
KRAFT_API bool           ReadDir(const String& Path, Array<FileInfo>& OutFiles);
KRAFT_API FileMMapHandle MMap(const String& Path);

} // namespace filesystem

}; // namespace kraft