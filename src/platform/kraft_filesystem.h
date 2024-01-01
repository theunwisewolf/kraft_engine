#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"

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

// File helpers
KRAFT_API bool OpenFile(const TCHAR* path, int mode, bool binary, FileHandle* out);
KRAFT_API bool OpenFile(const TString& Path, int Mode, bool Binary, FileHandle* Out);
KRAFT_API uint64 GetFileSize(FileHandle* handle);
KRAFT_API void CloseFile(FileHandle* handle);

// Returns true if the file exists at "path"
KRAFT_API bool FileExists(const TCHAR* path);
KRAFT_API bool FileExists(const TString& path);

// Very basic for now, just replaces all windows path separators
// with unix path separators
KRAFT_API void CleanPath(const TCHAR* path, TCHAR* out);
KRAFT_API TString CleanPath(const TString& Path);

// Returns the directory without the filename 
KRAFT_API void Basename(const TCHAR* path, TCHAR* out);
KRAFT_API TString Basename(const TString& Path);

// outBuffer, if null is allocated & must be freed by the caller
KRAFT_API bool ReadAllBytes(FileHandle* handle, uint8** outBuffer, uint64* bytesRead = nullptr);
KRAFT_API bool ReadAllBytes(const TCHAR* path, uint8** outBuffer, uint64* bytesRead = nullptr);

}

}