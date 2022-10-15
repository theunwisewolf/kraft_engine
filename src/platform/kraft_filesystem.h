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

namespace FileSystem
{

bool OpenFile(const char* path, int mode, bool binary, FileHandle* out);
void CloseFile(FileHandle* handle);
bool FileExists(const char* path);

// outBuffer, if null is allocated
// outBuffer must be freed by the caller
bool ReadAllBytes(FileHandle* handle, uint8** outBuffer, uint64* bytesRead);

}

}