#include <platform/kraft_filesystem.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <containers/kraft_array.h>
#include <core/kraft_asserts.h>
#include <core/kraft_string.h>
#include <core/kraft_log.h>
#include <platform/kraft_filesystem_types.h>

namespace kraft::filesystem {

FileMMapHandle MMap(const String& Path)
{
    HANDLE File =
        CreateFileA(*Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (File == INVALID_HANDLE_VALUE)
    {
        return { 0 };
    }

    HANDLE FileMapping = CreateFileMapping(File, NULL, PAGE_READONLY, 0, 0, NULL);
    KASSERT(FileMapping != INVALID_HANDLE_VALUE);

    LPVOID FileMapView = MapViewOfFile(FileMapping, FILE_MAP_READ, 0, 0, 0);
    KASSERT(FileMapView != nullptr);

    FileMMapHandle Out;
    Out.Handle = FileMapView;
    Out.Size = (uint64)(::GetFileSize(File, NULL));

    return Out;
}

bool ReadDir(const String& Path, Array<FileInfo>& OutFiles)
{
    String Dir = Path;
    if (!Dir.EndsWith("/*") && !Dir.EndsWith("\\*"))
    {
        Dir += "/*";
    }

    WIN32_FIND_DATA FindData;
    HANDLE          hFind = FindFirstFile(*Dir, &FindData);
    LARGE_INTEGER   FileSize;

    if (INVALID_HANDLE_VALUE == hFind)
    {
        kraft::String ErrorMessage(1024, 0);
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            *ErrorMessage,
            ErrorMessage.Length,
            NULL
        );

        KERROR("[Win32]: FindFirstFile failed with error %s", *ErrorMessage);
        return false;
    }

    do
    {
        if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // KDEBUG("[Dir]: %s", FindData.cFileName);
        }
        else
        {
            FileSize.LowPart = FindData.nFileSizeLow;
            FileSize.HighPart = FindData.nFileSizeHigh;
            // KDEBUG("  %s   %ld bytes", FindData.cFileName, FileSize.QuadPart);

            OutFiles.Push({ .Name = FindData.cFileName, .FileSize = (uint64)FileSize.QuadPart });
        }
    } while (FindNextFile(hFind, &FindData) != 0);

    FindClose(hFind);
    return true;
}

}