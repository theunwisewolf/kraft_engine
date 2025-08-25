#pragma once

namespace kraft::filesystem {
struct FileInfo
{
    String8 Name;
    uint64  FileSize;
};

struct FileHandle
{
    FILE* Handle;
};

struct FileMMapHandle
{
    void*  Handle;
    uint64 Size;
};

enum FileOpenMode
{
    FILE_OPEN_MODE_READ = 0x1,
    FILE_OPEN_MODE_WRITE = 0x2,
    FILE_OPEN_MODE_APPEND = 0x4,
};
} // namespace kraft::filesystem