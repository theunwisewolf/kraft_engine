#include "kraft_filesystem.h"

#include "core/kraft_log.h"
#include "core/kraft_string.h"
#include "core/kraft_asserts.h"
#include "core/kraft_memory.h"

#include <cerrno>

namespace kraft
{

namespace filesystem
{

bool OpenFile(const TString& Path, int Mode, bool Binary, FileHandle* Out)
{
    return OpenFile(Path.Data(), Mode, Binary, Out);
}

bool OpenFile(const TCHAR* path, int mode, bool binary, FileHandle* out)
{
    out->Handle = {};

    const TCHAR* modeString;
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

#if UNICODE
    #ifdef KRAFT_PLATFORM_WINDOWS
        out->Handle = _wfopen(path, modeString);
    #else
        char mbstrPath[256];
        wcstombs(mbstrPath, path, sizeof(mbstrPath));

        char mbstrMode[32];
        wcstombs(mbstrPath, modeString, sizeof(mbstrMode));
        
        out->Handle = fopen(mbstrPath, mbstrMode);
    #endif
#else
    out->Handle = fopen(path, modeString);
#endif
    if (!out->Handle)
    {
        KERROR("[FileSystem::OpenFile]: Failed to open %s with error %s", path, StrError(errno));
        return false;
    }

    return true;
}

uint64 GetFileSize(FileHandle* handle)
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

bool FileExists(const TCHAR* path)
{
    FileHandle temp;
    if (OpenFile(path, FILE_OPEN_MODE_READ, true, &temp))
    {
        CloseFile(&temp);
        return true;
    }

    return false;
}

bool FileExists(const TString& Path)
{
    return FileExists(Path.Data());
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

    size_t read = fread(*outBuffer, 1, size, (FILE*)handle->Handle);

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

bool ReadAllBytes(const TCHAR* path, uint8** outBuffer, uint64* bytesRead)
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

void CleanPath(const TCHAR* path, TCHAR* out)
{
    uint64 Length = StringLength(path);
    for (int i = 0; i < Length; i++)
    {
        if (path[i] == '\\')
        {
            out[i] = '/';
        }
        else
        {
            out[i] = path[i];
        }
    }
}

TString CleanPath(const TString& Path)
{
    TString Output(Path.Length, 0);
    for (int i = 0; i < Path.Length; i++)
    {
        if (Path[i] == '\\')
        {
            Output[i] = '/';
        }
        else
        {
            Output[i] = Path[i];
        }
    }

    return Output;
}

void Basename(const TCHAR* path, TCHAR* out)
{
    uint64 Length = StringLength(path);
    for (int i = Length - 1; i >= 0; i--)
    {
        if (path[i] == '/' || path[i] == '\\')
        {
            StringNCopy(out, path, i);
            return;
        }
    }
}

TString Basename(const TString& Path)
{
    for (int i = Path.Length - 1; i >= 0; i--)
    {
        if (Path[i] == '/' || Path[i] == '\\')
        {
            return TString(Path, 0, i);
        }
    }

    return Path;
}

}

}