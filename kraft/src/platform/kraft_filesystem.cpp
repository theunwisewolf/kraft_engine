#include "kraft_filesystem.h"

#include "core/kraft_asserts.h"
#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_string.h"

#include <cerrno>

namespace kraft {

namespace filesystem {

bool OpenFile(const String& Path, int Mode, bool Binary, FileHandle* Out)
{
    return OpenFile(Path.Data(), Mode, Binary, Out);
}

bool OpenFile(const char* Path, int Mode, bool binary, FileHandle* out)
{
    out->Handle = {};

    const char* ModeString;
    if (Mode & FILE_OPEN_MODE_APPEND)
    {
        ModeString = binary ? "a+b" : "a+";
    }
    else if ((Mode & FILE_OPEN_MODE_READ) && (Mode & FILE_OPEN_MODE_WRITE))
    {
        ModeString = binary ? "w+b" : "w+";
    }
    else if ((Mode & FILE_OPEN_MODE_READ) && (Mode & FILE_OPEN_MODE_WRITE) == 0)
    {
        ModeString = binary ? "r+b" : "r";
    }
    else if ((Mode & FILE_OPEN_MODE_READ) == 0 && (Mode & FILE_OPEN_MODE_WRITE))
    {
        ModeString = binary ? "wb" : "w";
    }
    else
    {
        KERROR("[FileSystem::OpenFile] Invalid mode %d", Mode);
        return false;
    }

#if UNICODE && defined(KRAFT_PLATFORM_WINDOWS)
    WString WideStringPath = MultiByteStringToWideCharString(Path);
    WString WideStringMode = MultiByteStringToWideCharString(ModeString);
    out->Handle = _wfopen(*WideStringPath, *WideStringMode);
#else
    out->Handle = fopen(Path, ModeString);
#endif
    if (!out->Handle)
    {
        KERROR("[FileSystem::OpenFile]: Failed to open %s with error %s", Path, StrError(errno));
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

bool FileExists(const String& Path)
{
    return FileExists(Path.Data());
}

// outBuffer, if null is allocated
// outBuffer must be freed by the caller
bool ReadAllBytes(FileHandle* handle, uint8** outBuffer, uint64* bytesRead)
{
    if (!handle->Handle)
        return false;

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

bool WriteFile(FileHandle* Handle, const uint8* Buffer, uint64 Size)
{
    if (!Handle->Handle)
        return false;

    fwrite(Buffer, sizeof(uint8), Size, (FILE*)Handle->Handle);

    return true;
}

bool WriteFile(FileHandle* Handle, const Buffer& Buffer)
{
    if (!Handle->Handle)
        return false;

    fwrite(*Buffer, sizeof(Buffer::ValueType), Buffer.Length, (FILE*)Handle->Handle);

    return true;
}

void CleanPath(const char* path, char* out)
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

String CleanPath(const String& Path)
{
    String Output(Path.Length, 0);
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

void Dirname(const char* path, char* out)
{
    uint64 Length = StringLength(path);
    for (int i = (int)Length - 1; i >= 0; i--)
    {
        if (path[i] == '/' || path[i] == '\\')
        {
            StringNCopy(out, path, i);
            return;
        }
    }
}

String Dirname(const String& Path)
{
    for (int i = (int)Path.Length - 1; i >= 0; i--)
    {
        if (Path[i] == '/' || Path[i] == '\\')
        {
            return String(Path, 0, i);
        }
    }

    return Path;
}

void Basename(const char* path, char* out)
{
    uint64 Length = StringLength(path);
    for (int i = (int)Length - 1; i >= 0; i--)
    {
        if (path[i] == '/' || path[i] == '\\')
        {
            StringNCopy(out, path + i + 1, Length - i + 1);
            return;
        }
    }
}

String Basename(const String& Path)
{
    for (int i = (int)Path.Length - 1; i >= 0; i--)
    {
        if (Path[i] == '/' || Path[i] == '\\')
        {
            return String(Path, i + 1, Path.Length - i + 1);
        }
    }

    return Path;
}

}

}