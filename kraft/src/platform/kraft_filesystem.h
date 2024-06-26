#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "containers/kraft_buffer.h"

#include <stdio.h>

namespace kraft
{

struct FileHandle
{
    void* Handle;
};

struct FileMMapHandle
{
    void*  Handle;
    uint64 Size;
};

enum FileOpenMode
{
    FILE_OPEN_MODE_READ   = 0x1,
    FILE_OPEN_MODE_WRITE  = 0x2,
    FILE_OPEN_MODE_APPEND = 0x4,
};

namespace filesystem
{

struct FileInfo
{
    String Name;
    uint64 FileSize;
};

// File helpers
KRAFT_API bool OpenFile(const char* path, int mode, bool binary, FileHandle* out);
KRAFT_API bool OpenFile(const String& Path, int Mode, bool Binary, FileHandle* Out);
KRAFT_API uint64 GetFileSize(FileHandle* handle);
KRAFT_API void CloseFile(FileHandle* handle);

// Returns true if the file exists at "path"
KRAFT_API bool FileExists(const char* path);
KRAFT_API bool FileExists(const String& path);

// Very basic for now, just replaces all windows path separators
// with unix path separators
KRAFT_API void CleanPath(const char* path, char* out);
KRAFT_API String CleanPath(const String& Path);

// Returns the directory without the filename
KRAFT_API void Dirname(const char* path, char* out);
KRAFT_API String Dirname(const String& Path);

// Returns the filename without the directory
KRAFT_API void Basename(const char* path, char* out);
KRAFT_API String Basename(const String& Path);

// outBuffer, if null is allocated & must be freed by the caller
KRAFT_API bool ReadAllBytes(FileHandle* handle, uint8** outBuffer, uint64* bytesRead = nullptr);
KRAFT_API bool ReadAllBytes(const char* path, uint8** outBuffer, uint64* bytesRead = nullptr);
KRAFT_API bool WriteFile(FileHandle* Handle, const uint8* Buffer, uint64 Size);
KRAFT_API bool WriteFile(FileHandle* Handle, const kraft::Buffer& Buffer);

// Platform dependent APIs
KRAFT_API bool ReadDir(const String& Path, Array<FileInfo>& OutFiles);
KRAFT_API FileMMapHandle MMap(const String& Path);

}

};