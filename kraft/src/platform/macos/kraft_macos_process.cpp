#include <platform/kraft_process.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_threading.h>

#include <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <core/kraft_core_includes.h>

extern char** environ;

namespace kraft {

struct MacProcessPipe {
    int file; // the read end, -1 once it is closed
    u8* data;
    u64 count;
    u64 capacity;
};

struct Process {
    ArenaAllocator* arena;
    pid_t id;
    int events; // a kqueue told about the program exiting and about output arriving
    bool detached;
    MacProcessPipe output;
    MacProcessPipe errors;
    u64 output_limit;
    bool exited;
    bool reaped;
    i32 exit_code;
};

static bool MacIsProgram(const char* path) {
    struct stat info;

    return stat(path, &info) == 0 && S_ISREG(info.st_mode) && access(path, X_OK) == 0;
}

// A program given only by name is looked up on the PATH, as a shell would. An app started from Finder gets a
// PATH of only the system's folders, so where Homebrew and installers put programs is tried after it.
static String8 MacFindProgram(ArenaAllocator* arena, String8 program) {
    for (u64 index = 0; index < program.count; index++) {
        if (program.ptr[index] == '/') {
            return StringCopy(arena, program);
        }
    }

    const char* path = getenv("PATH");
    String8 directories = StringFormat(arena, "%s:/usr/local/bin:/opt/homebrew/bin", path ? path : "/usr/bin:/bin:/usr/sbin:/sbin");
    u64 start = 0;
    while (start <= directories.count) {
        u64 end = start;
        while (end < directories.count && directories.ptr[end] != ':') {
            end++;
        }

        if (end > start) {
            String8 candidate = StringFormat(arena, "%.*s/%S", (int)(end - start), directories.ptr + start, program);
            if (MacIsProgram((const char*)candidate.ptr)) {
                return candidate;
            }
        }

        start = end + 1;
    }

    return String8{};
}

static u64 MacEnvironmentNameLength(const char* entry) {
    u64 length = 0;
    while (entry[length] && entry[length] != '=') {
        length++;
    }

    return length;
}

// This process's environment with the options' entries set on top
static char** MacEnvironment(ArenaAllocator* arena, ProcessOptions* options) {
    u64 parent_count = 0;
    while (environ[parent_count]) {
        parent_count++;
    }

    char** entries = ArenaPushArray(arena, char*, parent_count + options->environment_count + 1);
    u64 entry_count = 0;
    for (u32 index = 0; index < options->environment_count; index++) {
        entries[entry_count++] = (char*)StringCopy(arena, options->environment[index]).ptr;
    }

    u64 added_count = entry_count;
    for (u64 parent = 0; parent < parent_count; parent++) {
        const char* entry = environ[parent];
        u64 name_length = MacEnvironmentNameLength(entry);
        bool replaced = false;
        for (u64 index = 0; index < added_count && !replaced; index++) {
            replaced = MacEnvironmentNameLength(entries[index]) == name_length && strncmp(entries[index], entry, name_length) == 0;
        }

        if (!replaced) {
            entries[entry_count++] = (char*)entry;
        }
    }

    entries[entry_count] = nullptr;

    return entries;
}

// Takes what the pipe holds without waiting for more; returns true if anything came
static bool MacProcessDrain(Process* process, MacProcessPipe* pipe) {
    bool moved = false;
    while (pipe->file >= 0) {
        if (pipe->capacity - pipe->count < KRAFT_SIZE_KB(4)) {
            u64 capacity = pipe->capacity ? pipe->capacity * 2 : KRAFT_SIZE_KB(16);
            u8* grown = ArenaPushArrayNoZero(process->arena, u8, capacity);
            if (pipe->count) {
                MemCpy(grown, pipe->data, pipe->count);
            }
            pipe->data = grown;
            pipe->capacity = capacity;
        }

        // Past the limit everything is still read, so the program never stalls, and dropped
        u8 dropped[KRAFT_SIZE_KB(4)];
        bool keeping = process->output_limit == 0 || pipe->count < process->output_limit;
        u8* destination = keeping ? pipe->data + pipe->count : dropped;
        u64 room = keeping ? pipe->capacity - pipe->count : sizeof(dropped);
        if (keeping && process->output_limit > 0 && room > process->output_limit - pipe->count) {
            room = process->output_limit - pipe->count;
        }

        ssize_t got = read(pipe->file, destination, room);
        if (got < 0 && errno == EINTR) {
            continue;
        }

        if (got < 0 && errno == EAGAIN) {
            break;
        }

        if (got <= 0) {
            close(pipe->file);
            pipe->file = -1;
            break;
        }

        if (keeping) {
            pipe->count += (u64)got;
        }
        moved = true;
    }

    return moved;
}

// Whether the program has exited, leaving it unreaped: until it is, neither its id nor its process group's can go
// to another program, so the group can still be ended
static bool MacProcessExited(Process* process) {
    if (process->exited) {
        return true;
    }

    siginfo_t info = {};
    int result = waitid(P_PID, (id_t)process->id, &info, WEXITED | WNOHANG | WNOWAIT);
    while (result < 0 && errno == EINTR) {
        result = waitid(P_PID, (id_t)process->id, &info, WEXITED | WNOHANG | WNOWAIT);
    }

    if (result != 0 || info.si_pid != process->id) {
        return false;
    }

    process->exited = true;
    process->exit_code = info.si_code == CLD_EXITED ? info.si_status : 128 + info.si_status;

    return true;
}

static void MacProcessReap(Process* process) {
    int status = 0;
    while (!process->reaped && waitpid(process->id, &status, 0) < 0 && errno == EINTR) {
    }

    process->reaped = true;
}

// Nothing waits on a program started to outlive this one, so the system's queue reaps it once it exits, or it would
// stay a zombie for as long as this process runs
static void MacProcessReapWhenDone(pid_t id) {
    if (waitpid(id, nullptr, WNOHANG) == id) {
        return;
    }

    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)id, DISPATCH_PROC_EXIT, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
    if (!source) {
        return;
    }

    dispatch_source_set_event_handler(source, ^{
      waitpid(id, nullptr, WNOHANG);
      dispatch_source_cancel(source);
    });
    dispatch_source_set_cancel_handler(source, ^{
      dispatch_release(source);
    });
    dispatch_resume(source);
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
    process->events = -1;
    process->output.file = -1;
    process->errors.file = -1;

    TempArena scratch = ScratchBegin(&arena, 1);
    String8 program = MacFindProgram(scratch.arena, options->program);
    if (program.count == 0) {
        *out_status = PROCESS_START_STATUS_NOT_FOUND;
        ScratchEnd(scratch);
        DestroyArena(arena);
        return nullptr;
    }

    char** arguments = ArenaPushArray(scratch.arena, char*, options->argument_count + 2);
    arguments[0] = (char*)program.ptr;
    for (u32 index = 0; index < options->argument_count; index++) {
        arguments[index + 1] = (char*)StringCopy(scratch.arena, options->arguments[index]).ptr;
    }

    char** environment = options->environment_count > 0 ? MacEnvironment(scratch.arena, options) : environ;
    bool capture = options->capture_output && !options->detached;

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);

    // Nothing of this process's is handed down but what is set up here, as Windows' handle list does
    short flags = POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK;
    if (options->detached) {
        // A session of its own, so it outlives this process
        flags |= POSIX_SPAWN_SETSID;
    } else {
        // A process group of its own, so ProcessKill can end everything it started too
        flags |= POSIX_SPAWN_SETPGROUP;
        posix_spawnattr_setpgroup(&attributes, 0);
    }

    posix_spawnattr_setflags(&attributes, flags);
    sigset_t no_signals;
    sigemptyset(&no_signals);
    posix_spawnattr_setsigmask(&attributes, &no_signals);
    sigset_t default_signals;
    sigfillset(&default_signals);
    sigdelset(&default_signals, SIGKILL);
    sigdelset(&default_signals, SIGSTOP);
    posix_spawnattr_setsigdefault(&attributes, &default_signals);

    int output_pipe[2] = {-1, -1};
    int errors_pipe[2] = {-1, -1};
    if (capture) {
        pipe(output_pipe);
        pipe(errors_pipe);
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_adddup2(&actions, output_pipe[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, errors_pipe[1], STDERR_FILENO);
    } else if (options->detached) {
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    } else {
        posix_spawn_file_actions_addinherit_np(&actions, STDIN_FILENO);
        posix_spawn_file_actions_addinherit_np(&actions, STDOUT_FILENO);
        posix_spawn_file_actions_addinherit_np(&actions, STDERR_FILENO);
    }

    if (options->working_directory.count > 0) {
        posix_spawn_file_actions_addchdir_np(&actions, (const char*)StringCopy(scratch.arena, options->working_directory).ptr);
    }

    pid_t id = 0;
    int error = posix_spawn(&id, (const char*)program.ptr, &actions, &attributes, arguments, environment);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    if (capture) {
        close(output_pipe[1]);
        close(errors_pipe[1]);
    }

    ScratchEnd(scratch);

    if (error != 0) {
        // A working directory that is not there fails the same way as a program that is not
        *out_status = (error == ENOENT || error == ENOTDIR || error == EACCES) && !MacIsProgram((const char*)program.ptr) ? PROCESS_START_STATUS_NOT_FOUND : PROCESS_START_STATUS_FAILED;
        if (capture) {
            close(output_pipe[0]);
            close(errors_pipe[0]);
        }

        DestroyArena(arena);
        return nullptr;
    }

    process->id = id;
    process->events = kqueue();
    struct kevent changes[3];
    int change_count = 0;
    if (capture) {
        process->output.file = output_pipe[0];
        process->errors.file = errors_pipe[0];
        fcntl(process->output.file, F_SETFL, O_NONBLOCK);
        fcntl(process->errors.file, F_SETFL, O_NONBLOCK);
        fcntl(process->output.file, F_SETFD, FD_CLOEXEC);
        fcntl(process->errors.file, F_SETFD, FD_CLOEXEC);
        EV_SET(&changes[change_count++], (uintptr_t)process->output.file, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        EV_SET(&changes[change_count++], (uintptr_t)process->errors.file, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    }

    // Last, since a program that has already exited cannot be watched for it and the changes after one that fails
    // are not made; the first wait finds out it exited
    EV_SET(&changes[change_count++], (uintptr_t)id, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, nullptr);
    kevent(process->events, changes, change_count, nullptr, 0, nullptr);
    *out_status = PROCESS_START_STATUS_OK;

    return process;
}

bool ProcessWait(Process* process, u64 timeout_ms) {
    if (process->exited) {
        return true;
    }

    bool forever = timeout_ms == KRAFT_WAIT_FOREVER;
    f64 deadline = forever ? 0.0 : Platform::GetAbsoluteTime() + timeout_ms / 1000.0;
    for (;;) {
        MacProcessExited(process);

        bool moved = MacProcessDrain(process, &process->output);
        moved = MacProcessDrain(process, &process->errors) || moved;

        // Once the program has gone, what is in the pipes is all there is, even if something it started still has them open
        if (process->exited && !moved) {
            return true;
        }

        if (moved) {
            continue;
        }

        struct timespec wait = {};
        if (!forever) {
            f64 remaining = deadline - Platform::GetAbsoluteTime();
            if (remaining <= 0.0) {
                return false;
            }

            wait.tv_sec = (time_t)remaining;
            wait.tv_nsec = (long)((remaining - (f64)wait.tv_sec) * 1000000000.0);
        }

        // Wakes for output or for the program exiting; a short wait in case it exited before it could be watched
        struct timespec short_wait = {0, 50 * 1000000};
        if (forever || wait.tv_sec > 0 || wait.tv_nsec > short_wait.tv_nsec) {
            wait = short_wait;
        }

        struct kevent event;
        kevent(process->events, nullptr, 0, &event, 1, &wait);
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
    // The whole group, which ends what the program started even once the program itself has gone
    if (!process->detached) {
        kill(-process->id, SIGKILL);
    } else if (!process->exited) {
        kill(process->id, SIGKILL);
    }
}

void ProcessFree(Process* process) {
    if (!process) {
        return;
    }

    if (process->detached) {
        MacProcessReapWhenDone(process->id);
    } else {
        // What the program started goes with it, as closing its job does on Windows
        ProcessKill(process);
        f64 deadline = Platform::GetAbsoluteTime() + 5.0;
        while (!MacProcessExited(process) && Platform::GetAbsoluteTime() < deadline) {
            Platform::SleepMilliseconds(1);
        }

        if (process->exited) {
            MacProcessReap(process);
        }
    }

    if (process->output.file >= 0) {
        close(process->output.file);
    }

    if (process->errors.file >= 0) {
        close(process->errors.file);
    }

    if (process->events >= 0) {
        close(process->events);
    }

    DestroyArena(process->arena);
}

u32 ProcessCurrentId() {
    return (u32)getpid();
}

bool ProcessWaitById(u32 process_id, u64 timeout_ms) {
    int events = kqueue();
    struct kevent change;
    EV_SET(&change, (uintptr_t)process_id, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, nullptr);
    if (kevent(events, &change, 1, nullptr, 0, nullptr) < 0) {
        close(events);
        return errno == ESRCH;
    }

    struct timespec wait = {(time_t)(timeout_ms / 1000), (long)((timeout_ms % 1000) * 1000000)};
    struct kevent event;
    int count = kevent(events, nullptr, 0, &event, 1, timeout_ms == KRAFT_WAIT_FOREVER ? nullptr : &wait);
    close(events);

    return count > 0;
}

} // namespace kraft
