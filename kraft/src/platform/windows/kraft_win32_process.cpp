#include <platform/kraft_process.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_threading.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <stdlib.h>

#include <core/kraft_core_includes.h>

namespace kraft {

struct Win32ProcessPipe {
    HANDLE handle;
    u8* data;
    u64 count;
    u64 capacity;
    bool closed;
};

struct Process {
    ArenaAllocator* arena;
    HANDLE handle;
    HANDLE job;
    bool detached;
    Win32ProcessPipe output;
    Win32ProcessPipe errors;
    u64 output_limit;
    bool exited;
    i32 exit_code;
};

static WCHAR* Win32ProcessWide(ArenaAllocator* arena, String8 text) {
    i32 count = MultiByteToWideChar(CP_UTF8, 0, (const char*)text.ptr, (i32)text.count, nullptr, 0);
    WCHAR* wide = ArenaPushArray(arena, WCHAR, (u64)count + 1);
    MultiByteToWideChar(CP_UTF8, 0, (const char*)text.ptr, (i32)text.count, wide, count);

    return wide;
}

// The quoting CommandLineToArgvW undoes: backslashes are literal except in front of a quote
static u64 Win32QuoteArgument(char* out, String8 argument) {
    bool plain = argument.count > 0;
    for (u64 index = 0; index < argument.count && plain; index++) {
        char character = (char)argument.ptr[index];
        plain = character != ' ' && character != '\t' && character != '\n' && character != '\v' && character != '"';
    }

    if (plain) {
        MemCpy(out, argument.ptr, argument.count);
        return argument.count;
    }

    u64 count = 0;
    out[count++] = '"';
    u64 backslashes = 0;
    for (u64 index = 0; index < argument.count; index++) {
        char character = (char)argument.ptr[index];
        if (character == '\\') {
            backslashes++;
            continue;
        }
        u64 repeat = character == '"' ? backslashes * 2 + 1 : backslashes;
        for (u64 slash = 0; slash < repeat; slash++) {
            out[count++] = '\\';
        }
        out[count++] = character;
        backslashes = 0;
    }

    for (u64 slash = 0; slash < backslashes * 2; slash++) {
        out[count++] = '\\';
    }

    out[count++] = '"';

    return count;
}

// A program given only by name is looked up the way a shell would, with the extensions it would try
static String8 Win32FindProgram(ArenaAllocator* arena, String8 program) {
    for (u64 index = 0; index < program.count; index++) {
        if (program.ptr[index] == '/' || program.ptr[index] == '\\') {
            return program;
        }
    }

    WCHAR* name = Win32ProcessWide(arena, program);
    const WCHAR* extensions[] = {L".exe", L".com", L".cmd", L".bat"};
    WCHAR found[MAX_PATH * 2];
    for (u32 index = 0; index < KRAFT_C_ARRAY_SIZE(extensions); index++) {
        DWORD length = SearchPathW(nullptr, name, extensions[index], (DWORD)KRAFT_C_ARRAY_SIZE(found), found, nullptr);
        if (length > 0 && length < KRAFT_C_ARRAY_SIZE(found)) {
            i32 bytes = WideCharToMultiByte(CP_UTF8, 0, found, (i32)length, nullptr, 0, nullptr, nullptr);
            String8 path = ArenaPushString8Empty(arena, (u64)bytes);
            WideCharToMultiByte(CP_UTF8, 0, found, (i32)length, (char*)path.ptr, bytes, nullptr, nullptr);

            return path;
        }
    }

    return String8{};
}

static bool Win32IsBatchFile(String8 path) {
    return StringEndsWith(path, String8Raw(".cmd")) || StringEndsWith(path, String8Raw(".CMD")) || StringEndsWith(path, String8Raw(".bat")) || StringEndsWith(path, String8Raw(".BAT"));
}

// Batch files only run through cmd.exe, whose /s /c takes the whole line in one pair of quotes
static String8 Win32CommandLine(ArenaAllocator* arena, String8 program, ProcessOptions* options) {
    bool batch = Win32IsBatchFile(program);
    u64 capacity = program.count * 2 + 32;
    for (u32 index = 0; index < options->argument_count; index++) {
        capacity += options->arguments[index].count * 2 + 3;
    }

    char* line = ArenaPushArrayNoZero(arena, char, capacity);
    u64 count = 0;
    if (batch) {
        MemCpy(line, "cmd.exe /d /s /c \"", 18);
        count += 18;
    }

    line[count++] = '"';
    MemCpy(line + count, program.ptr, program.count);
    count += program.count;
    line[count++] = '"';
    for (u32 index = 0; index < options->argument_count; index++) {
        line[count++] = ' ';
        count += Win32QuoteArgument(line + count, options->arguments[index]);
    }

    if (batch) {
        line[count++] = '"';
    }

    return String8{{(u8*)line}, count};
}

static u64 Win32EnvironmentNameLength(const WCHAR* entry) {
    // Entries like "=C:=C:\dir" start with the '=' that is part of their name
    u64 length = 1;
    while (entry[length] && entry[length] != L'=') {
        length++;
    }

    return length;
}

static int Win32CompareEnvironmentEntries(const void* left, const void* right) {
    const WCHAR* left_entry = *(const WCHAR* const*)left;
    const WCHAR* right_entry = *(const WCHAR* const*)right;
    int order = CompareStringOrdinal(left_entry, (int)Win32EnvironmentNameLength(left_entry), right_entry, (int)Win32EnvironmentNameLength(right_entry), TRUE);

    return order - CSTR_EQUAL;
}

// This process's environment with the options' entries set on top, sorted by name as CreateProcess wants it
static WCHAR* Win32EnvironmentBlock(ArenaAllocator* arena, ProcessOptions* options) {
    WCHAR* parent = GetEnvironmentStringsW();
    u64 parent_count = 0;
    for (WCHAR* entry = parent; *entry; entry += wcslen(entry) + 1) {
        parent_count++;
    }

    const WCHAR** entries = ArenaPushArray(arena, const WCHAR*, parent_count + options->environment_count);
    u32 entry_count = 0;
    for (u32 index = 0; index < options->environment_count; index++) {
        entries[entry_count++] = Win32ProcessWide(arena, options->environment[index]);
    }

    u32 added_count = entry_count;
    for (WCHAR* entry = parent; *entry; entry += wcslen(entry) + 1) {
        bool replaced = false;
        for (u32 index = 0; index < added_count && !replaced; index++) {
            replaced = CompareStringOrdinal(entry, (int)Win32EnvironmentNameLength(entry), entries[index], (int)Win32EnvironmentNameLength(entries[index]), TRUE) == CSTR_EQUAL;
        }
        if (!replaced) {
            u64 length = wcslen(entry);
            WCHAR* copy = ArenaPushArray(arena, WCHAR, length + 1);
            MemCpy(copy, entry, length * sizeof(WCHAR));
            entries[entry_count++] = copy;
        }
    }

    FreeEnvironmentStringsW(parent);
    qsort(entries, entry_count, sizeof(entries[0]), Win32CompareEnvironmentEntries);

    u64 total = 1;
    for (u32 index = 0; index < entry_count; index++) {
        total += wcslen(entries[index]) + 1;
    }

    WCHAR* block = ArenaPushArray(arena, WCHAR, total);
    u64 at = 0;
    for (u32 index = 0; index < entry_count; index++) {
        u64 length = wcslen(entries[index]);
        MemCpy(block + at, entries[index], length * sizeof(WCHAR));
        at += length + 1;
    }

    return block;
}

// Takes what the pipe holds without waiting for more; returns true if anything came
static bool Win32ProcessDrain(Process* process, Win32ProcessPipe* pipe) {
    if (pipe->closed || !pipe->handle) {
        return false;
    }

    DWORD available = 0;
    if (!PeekNamedPipe(pipe->handle, nullptr, 0, nullptr, &available, nullptr)) {
        pipe->closed = true;
        return false;
    }

    if (available == 0) {
        return false;
    }

    if (pipe->count + available > pipe->capacity) {
        u64 capacity = pipe->capacity * 2 > pipe->count + available + KRAFT_SIZE_KB(4) ? pipe->capacity * 2 : pipe->count + available + KRAFT_SIZE_KB(4);
        u8* grown = ArenaPushArrayNoZero(process->arena, u8, capacity);
        if (pipe->count) {
            MemCpy(grown, pipe->data, pipe->count);
        }
        pipe->data = grown;
        pipe->capacity = capacity;
    }

    DWORD read = 0;
    if (!ReadFile(pipe->handle, pipe->data + pipe->count, available, &read, nullptr)) {
        pipe->closed = true;
        return false;
    }

    u64 kept = read;
    if (process->output_limit > 0) {
        u64 room = process->output_limit > pipe->count ? process->output_limit - pipe->count : 0;
        kept = read < room ? read : room;
    }

    pipe->count += kept;

    return read > 0;
}

Process* ProcessStart(ProcessOptions* options, ProcessStartStatus* out_status) {
    ProcessStartStatus ignored_status;
    if (!out_status) {
        out_status = &ignored_status;
    }

    ArenaAllocator* arena = ArenaAlloc();
    Process* process = ArenaPush(arena, Process);
    process->arena = arena;
    process->detached = options->detached;
    process->output_limit = options->output_limit;
    process->exit_code = -1;

    TempArena scratch = ScratchBegin(&arena, 1);
    String8 program = Win32FindProgram(scratch.arena, options->program);
    if (program.count == 0) {
        *out_status = PROCESS_START_STATUS_NOT_FOUND;
        ScratchEnd(scratch);
        DestroyArena(arena);
        return nullptr;
    }
    WCHAR* command_line = Win32ProcessWide(scratch.arena, Win32CommandLine(scratch.arena, program, options));
    WCHAR* directory = options->working_directory.count > 0 ? Win32ProcessWide(scratch.arena, options->working_directory) : nullptr;
    WCHAR* environment = options->environment_count > 0 ? Win32EnvironmentBlock(scratch.arena, options) : nullptr;
    bool capture = options->capture_output && !options->detached;

    DWORD flags = CREATE_UNICODE_ENVIRONMENT;
    if (capture || options->detached) {
        flags |= CREATE_NO_WINDOW;
    }

    if (!options->detached) {
        process->job = CreateJobObjectW(nullptr, nullptr);
        if (process->job) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(process->job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        }

        flags |= CREATE_SUSPENDED;
    }

    STARTUPINFOEXW startup = {};
    startup.StartupInfo.cb = sizeof(startup);
    HANDLE output_write = nullptr;
    HANDLE errors_write = nullptr;
    HANDLE input = nullptr;
    LPPROC_THREAD_ATTRIBUTE_LIST attributes = nullptr;
    if (capture) {
        SECURITY_ATTRIBUTES inherit = {sizeof(inherit), nullptr, TRUE};
        CreatePipe(&process->output.handle, &output_write, &inherit, 0);
        CreatePipe(&process->errors.handle, &errors_write, &inherit, 0);
        SetHandleInformation(process->output.handle, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(process->errors.handle, HANDLE_FLAG_INHERIT, 0);
        input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);

        SIZE_T attributes_size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attributes_size);
        attributes = (LPPROC_THREAD_ATTRIBUTE_LIST)ArenaPushArray(scratch.arena, u8, attributes_size);
        InitializeProcThreadAttributeList(attributes, 1, 0, &attributes_size);

        // Restrict inheritance to exactly these three handles. With bInheritHandles=TRUE, CreateProcess otherwise copies
        // every inheritable handle in this process into the child, including pipe ends another thread just created for
        // a concurrent ProcessStart. A pipe only reports EOF once every copy of its write end is closed, so a stray copy
        // held by an unrelated child keeps that other reader blocked until our child exits.
        HANDLE inherited[3] = {input, output_write, errors_write};
        UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr);

        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = input;
        startup.StartupInfo.hStdOutput = output_write;
        startup.StartupInfo.hStdError = errors_write;
        startup.lpAttributeList = attributes;
        flags |= EXTENDED_STARTUPINFO_PRESENT;
    }

    PROCESS_INFORMATION started = {};
    bool ok = CreateProcessW(nullptr, command_line, nullptr, nullptr, capture, flags, environment, directory, &startup.StartupInfo, &started);
    DWORD start_error = ok ? 0 : GetLastError();
    if (attributes) {
        DeleteProcThreadAttributeList(attributes);
    }

    if (capture) {
        CloseHandle(output_write);
        CloseHandle(errors_write);
        CloseHandle(input);
    }

    ScratchEnd(scratch);

    if (!ok) {
        *out_status = start_error == ERROR_FILE_NOT_FOUND || start_error == ERROR_PATH_NOT_FOUND ? PROCESS_START_STATUS_NOT_FOUND : PROCESS_START_STATUS_FAILED;
        if (process->output.handle) {
            CloseHandle(process->output.handle);
        }

        if (process->errors.handle) {
            CloseHandle(process->errors.handle);
        }

        if (process->job) {
            CloseHandle(process->job);
        }

        DestroyArena(arena);
        return nullptr;
    }

    if (process->job) {
        AssignProcessToJobObject(process->job, started.hProcess);
    }

    if (flags & CREATE_SUSPENDED) {
        ResumeThread(started.hThread);
    }

    CloseHandle(started.hThread);
    process->handle = started.hProcess;
    *out_status = PROCESS_START_STATUS_OK;

    return process;
}

bool ProcessWait(Process* process, u64 timeout_ms) {
    if (process->exited) {
        return true;
    }

    bool forever = timeout_ms == KRAFT_WAIT_FOREVER;
    f64 deadline = forever ? 0.0 : Platform::GetAbsoluteTime() + timeout_ms / 1000.0;
    bool capturing = process->output.handle || process->errors.handle;
    for (;;) {
        bool exited = WaitForSingleObject(process->handle, 0) == WAIT_OBJECT_0;
        bool moved = Win32ProcessDrain(process, &process->output);
        moved = Win32ProcessDrain(process, &process->errors) || moved;
        // Once the program has gone, what is in the pipes is all there is, even if something it started still has them open
        if (exited && !moved) {
            DWORD exit_code = 1;
            GetExitCodeProcess(process->handle, &exit_code);
            process->exit_code = (i32)exit_code;
            process->exited = true;
            return true;
        }

        if (moved) {
            continue;
        }

        DWORD wait = INFINITE;
        if (!forever) {
            f64 remaining = deadline - Platform::GetAbsoluteTime();
            if (remaining <= 0.0) {
                return false;
            }
            wait = (DWORD)(remaining * 1000.0) + 1;
        }

        // A program writing its output has to be read while it runs, or it stalls on a full pipe
        if (capturing && wait > 10) {
            wait = 10;
        }

        WaitForSingleObject(process->handle, wait);
    }
}

i32 ProcessExitCode(Process* process) {
    return process->exited ? process->exit_code : -1;
}

String8 ProcessOutput(Process* process) {
    return String8{{process->output.data}, process->output.count};
}

String8 ProcessErrors(Process* process) {
    return String8{{process->errors.data}, process->errors.count};
}

void ProcessKill(Process* process) {
    if (process->job) {
        TerminateJobObject(process->job, 1);
    } else {
        TerminateProcess(process->handle, 1);
    }
}

void ProcessFree(Process* process) {
    if (!process) {
        return;
    }

    if (!process->detached && !process->exited) {
        ProcessKill(process);
        WaitForSingleObject(process->handle, 5000);
    }

    CloseHandle(process->handle);
    if (process->job) {
        CloseHandle(process->job);
    }

    if (process->output.handle) {
        CloseHandle(process->output.handle);
    }

    if (process->errors.handle) {
        CloseHandle(process->errors.handle);
    }

    DestroyArena(process->arena);
}

u32 ProcessCurrentId() {
    return (u32)GetCurrentProcessId();
}

bool ProcessWaitById(u32 process_id, u64 timeout_ms) {
    HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, process_id);
    if (!handle) {
        return GetLastError() == ERROR_INVALID_PARAMETER;
    }

    DWORD result = WaitForSingleObject(handle, timeout_ms == KRAFT_WAIT_FOREVER ? INFINITE : (DWORD)timeout_ms);
    CloseHandle(handle);

    return result == WAIT_OBJECT_0;
}

} // namespace kraft
