#include "kraft_filesystem.h"

#include "core/kraft_log.h"
#include "core/kraft_asserts.h"
#include "core/kraft_memory.h"

#include <cerrno>

namespace kraft
{

namespace filesystem
{

bool OpenFile(const char* path, int mode, bool binary, FileHandle* out)
{
    out->Handle = {};

    const char* modeString;
    if (mode & FILE_OPEN_MODE_APPEND)
    {
        modeString = binary ? "a+b" : "a+";
    }
    else if ((mode & FILE_OPEN_MODE_READ) && (mode & FILE_OPEN_MODE_WRITE))
    {
        modeString = binary ? "w+b" : "w+";
    }
    else if ((mode & FILE_OPEN_MODE_READ) && (mode & FILE_OPEN_MODE_WRITE) == 0)
    {
        modeString = binary ? "r+b" : "r";
    }
    else if ((mode & FILE_OPEN_MODE_READ) == 0 && (mode & FILE_OPEN_MODE_WRITE))
    {
        modeString = binary ? "wb" : "w";
    }
    else
    {
        KERROR("[FileSystem::OpenFile] Invalid mode %d", mode);
        return false;
    }

    out->Handle = fopen(path, modeString);
    if (!out->Handle)
    {
        KERROR("[FileSystem::OpenFile]: Failed with error %s", strerror(errno));
        return false;
    }

    return true;
}

uint64 GetSize(FileHandle* handle)
{
    KASSERT(handle->Handle);

    fseek((FILE*)handle->Handle, 0, SEEK_END);
    uint64 size = ftell((FILE*)handle->Handle);
    rewind((FILE*)handle->Handle);

    return size;
}

void CloseFile(FileHandle* handle)
{
    if (handle)
    {
        fclose((FILE*)handle->Handle);
        handle->Handle = 0;
    }
}

bool FileExists(const char* path)
{
    FileHandle temp;
    if (OpenFile(path, FILE_OPEN_MODE_READ, true, &temp))
    {
        CloseFile(&temp);
        return true;
    }

    return false;
}

// outBuffer, if null is allocated
// outBuffer must be freed by the caller
bool ReadAllBytes(FileHandle* handle, uint8** outBuffer, uint64* bytesRead)
{
    if (!handle->Handle) return false;

    fseek((FILE*)handle->Handle, 0, SEEK_END);
    uint64 size = ftell((FILE*)handle->Handle);
    rewind((FILE*)handle->Handle);

    if (!*outBuffer) 
        *outBuffer = (uint8*)Malloc(size, MEMORY_TAG_FILE_BUF);

    int read = fread(*outBuffer, 1, size, (FILE*)handle->Handle);
    // Not all bytes were read
    if (read != size)
    {
        return false;
    }

    if (bytesRead)
    {
        *bytesRead = read;
    }

    return true;
}

bool ReadAllBytes(const char* path, uint8** outBuffer, uint64* bytesRead)
{
    FileHandle handle;
    if (!OpenFile(path, FILE_OPEN_MODE_READ, true, &handle))
    {
        return false;
    }

    bool retval = ReadAllBytes(&handle, outBuffer, bytesRead);
    CloseFile(&handle);

    return retval;
}

}

}