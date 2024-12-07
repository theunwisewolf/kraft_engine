#pragma once

namespace kraft::filesystem {
struct FileInfo
{
    String Name;
    uint64 FileSize;
};

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
    FILE_OPEN_MODE_READ = 0x1,
    FILE_OPEN_MODE_WRITE = 0x2,
    FILE_OPEN_MODE_APPEND = 0x4,
};
}