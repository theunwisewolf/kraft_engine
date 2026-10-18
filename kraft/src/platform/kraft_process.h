#pragma once

#include <core/kraft_core.h>

namespace kraft {

struct Process;

struct ProcessOptions {
    // The program to run, looked up on the PATH when it is only a name like "git"
    String8 program;
    String8* arguments;
    u32 argument_count;

    // The directory the program runs in; this process's own when empty
    String8 working_directory;

    // "NAME=value" entries set for the program on top of this process's own environment
    String8* environment;
    u32 environment_count;

    // Reads the program's standard output and error, to be taken with ProcessOutput and ProcessErrors
    // Without it the program writes to this process's console
    bool capture_output;

    // The most captured output that is kept; anything past it is read and dropped, so the program never stalls
    // 0 keeps all of it
    u64 output_limit;

    // Leaves the program running after ProcessFree, for starting another app
    // Without it, ProcessFree ends the program and everything it started
    bool detached;
};

enum ProcessStartStatus {
    PROCESS_START_STATUS_OK,
    PROCESS_START_STATUS_NOT_FOUND, // the program doesn't exist, or isn't on the PATH
    PROCESS_START_STATUS_FAILED,
};

// Starts a program and returns it, or null if it could not be started
// `out_status` is optional and says why it could not
KRAFT_API Process* ProcessStart(ProcessOptions* options, ProcessStartStatus* out_status = nullptr);

// Waits up to `timeout_ms` for the program to exit, reading its captured output meanwhile
// Returns true once it has exited; 0 checks without waiting, KRAFT_WAIT_FOREVER waits until it exits
KRAFT_API bool ProcessWait(Process* process, u64 timeout_ms);

// Returns the exit code of a program that has exited, -1 before that
KRAFT_API i32 ProcessExitCode(Process* process);

// Returns everything the program has written to its standard output so far, valid until ProcessFree
KRAFT_API String8 ProcessOutput(Process* process);

// Returns everything the program has written to its standard error so far, valid until ProcessFree
KRAFT_API String8 ProcessErrors(Process* process);

// Ends the program and everything it started
KRAFT_API void ProcessKill(Process* process);

// Frees the process, ending the program first unless it was started detached
KRAFT_API void ProcessFree(Process* process);

// Returns the id the system knows this process by
KRAFT_API u32 ProcessCurrentId();

// Waits up to `timeout_ms` for the process with the id `process_id` to exit, such as one that started this one
// Returns true once it has exited, or when there is no process with that id
KRAFT_API bool ProcessWaitById(u32 process_id, u64 timeout_ms);

} // namespace kraft
