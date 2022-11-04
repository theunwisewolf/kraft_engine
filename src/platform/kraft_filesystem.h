#pragma once

#include "core/kraft_core.h"

#include <stdio.h>

namespace kraft
{

struct FileHandle
{
    void* Handle;
};

enum FileOpenMode
{
    FILE_OPEN_MODE_READ   = 0x1,
    FILE_OPEN_MODE_WRITE  = 0x2,
    FILE_OPEN_MODE_APPEND = 0x4,
};

namespace filesystem
{

KRAFT_API bool OpenFile(const char* path, int mode, bool binary, FileHandle* out);
KRAFT_API uint64 GetFileSize(FileHandle* handle);
KRAFT_API void CloseFile(FileHandle* handle);
KRAFT_API bool FileExists(const char* path);

// outBuffer, if null is allocated & must be freed by the caller
KRAFT_API bool ReadAllBytes(FileHandle* handle, uint8** outBuffer, uint64* bytesRead = nullptr);
KRAFT_API bool ReadAllBytes(const char* path, uint8** outBuffer, uint64* bytesRead = nullptr);

}

}