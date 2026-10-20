#include "kraft_macos.h"
#include "kraft_macos_cocoa.h"

#include <core/kraft_core.h>

#ifdef KRAFT_PLATFORM_MACOS

#include <dirent.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(KRAFT_GUI_APP)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>
#include <platform/kraft_window.h>
#endif

#include <core/kraft_allocators.h>
#include <core/kraft_log.h>
#include <core/kraft_string.h>
#include <core/kraft_strings.h>
#include <core/kraft_thread_context.h>
#include <platform/kraft_platform.h>
#include <platform/macos/kraft_macos_types.h>

namespace kraft {

PlatformState* Platform::State = nullptr;

// Select Graphic Rendition codes: foreground in the low 16 bits, background above them
const int Platform::ConsoleColorBlack = 30;
const int Platform::ConsoleColorLoWhite = 37;
const int Platform::ConsoleColorHiWhite = 97;
const int Platform::ConsoleColorLoRed = 31;
const int Platform::ConsoleColorHiRed = 91;
const int Platform::ConsoleColorLoGreen = 32;
const int Platform::ConsoleColorHiGreen = 92;
const int Platform::ConsoleColorLoBlue = 34;
const int Platform::ConsoleColorHiBlue = 94;
const int Platform::ConsoleColorLoYellow = 33;
const int Platform::ConsoleColorHiYellow = 93;
const int Platform::ConsoleColorLoCyan = 36;
const int Platform::ConsoleColorHiCyan = 96;
const int Platform::ConsoleColorLoMagenta = 35;
const int Platform::ConsoleColorHiMagenta = 95;
const int Platform::ConsoleColorLoGray = 37;
const int Platform::ConsoleColorHiGray = 90;

const int Platform::ConsoleColorBGBlack = 40 << 16;
const int Platform::ConsoleColorBGLoWhite = 47 << 16;
const int Platform::ConsoleColorBGHiWhite = 107 << 16;
const int Platform::ConsoleColorBGLoRed = 41 << 16;
const int Platform::ConsoleColorBGHiRed = 101 << 16;
const int Platform::ConsoleColorBGLoGreen = 42 << 16;
const int Platform::ConsoleColorBGHiGreen = 102 << 16;
const int Platform::ConsoleColorBGLoBlue = 44 << 16;
const int Platform::ConsoleColorBGHiBlue = 104 << 16;
const int Platform::ConsoleColorBGLoYellow = 43 << 16;
const int Platform::ConsoleColorBGHiYellow = 103 << 16;
const int Platform::ConsoleColorBGLoCyan = 46 << 16;
const int Platform::ConsoleColorBGHiCyan = 106 << 16;
const int Platform::ConsoleColorBGLoMagenta = 45 << 16;
const int Platform::ConsoleColorBGHiMagenta = 105 << 16;

static u64 s_start_nanoseconds;

bool Platform::Init(struct EngineConfig* config) {
    State = (PlatformState*)Malloc(sizeof(PlatformState), false);
    MemZero(State, sizeof(PlatformState));
    s_start_nanoseconds = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);

#if defined(KRAFT_GUI_APP)
    // GLFW would otherwise move the working directory into the bundle's Resources, which breaks relative paths
    // given on the command line
    glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
#endif

    return true;
}

bool Platform::PollEvents() {
#if defined(KRAFT_GUI_APP)
    return State->PrimaryWindow->PollEvents();
#else
    return false;
#endif
}

void Platform::Shutdown() {
#if defined(KRAFT_GUI_APP)
    DestroyPlatformWindow(State->PrimaryWindow);
#endif

    Free(State);
}

// ------------------------------------------
// Memory Specific Functions
// ------------------------------------------

void* Platform::MemReserve(u64 size) {
    void* region = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);

    return region == MAP_FAILED ? nullptr : region;
}

bool Platform::MemCommit(void* address, u64 size) {
    return mprotect(address, size, PROT_READ | PROT_WRITE) == 0;
}

// Maps fresh pages over the range, which gives its memory back and makes it read as zero once committed again,
// as decommitted memory does on Windows
void Platform::MemDecommit(void* address, u64 size) {
    mmap(address, size, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
}

void Platform::MemRelease(void* address, u64 size) {
    munmap(address, size);
}

u64 Platform::MemPageSize() {
    return (u64)getpagesize();
}

void* Platform::Malloc(u64 size, bool aligned) {
    return malloc(size);
}

void* Platform::Realloc(void* region, u64 size, bool aligned) {
    return realloc(region, size);
}

void Platform::Free(void* region) {
    free(region);
}

void* Platform::MemZero(void* region, u64 size) {
    return memset(region, 0, size);
}

void* Platform::MemCpy(void* dst, const void* src, u64 size) {
    return memcpy(dst, src, size);
}

void* Platform::MemSet(void* region, int value, u64 size) {
    return memset(region, value, size);
}

int Platform::MemCmp(const void* a, const void* b, u64 size) {
    return memcmp(a, b, size);
}

// ------------------------------------------
// Console Specific Functions
// ------------------------------------------

static void MacConsoleWriteColored(FILE* stream, const char* text, int color) {
    int foreground = color & 0xffff;
    int background = color >> 16;
    fprintf(stream, "\x1B[0;%d;%dm%s\x1B[0m", foreground ? foreground : 39, background ? background : 49, text);
}

void Platform::ConsoleSetColor(int color) {
    int foreground = color & 0xffff;
    int background = color >> 16;
    fprintf(stdout, "\x1B[%d;%dm", foreground ? foreground : 39, background ? background : 49);
}

void Platform::ConsoleOutputString(const char* text) {
    fputs(text, stdout);
}

void Platform::ConsoleOutputString(const char* text, int color) {
    MacConsoleWriteColored(stdout, text, color);
}

void Platform::ConsoleOutputStringError(const char* text) {
    fputs(text, stderr);
}

void Platform::ConsoleOutputStringError(const char* text, int color) {
    MacConsoleWriteColored(stderr, text, color);
}

void Platform::ConsoleResetFormatting() {
    fputs("\x1B[0m", stdout);
}

bool Platform::RandomBytes(void* Out, u64 Count) {
    arc4random_buf(Out, (size_t)Count);

    return true;
}

// One DER INTEGER from a 32-byte big-endian number: no leading zero bytes, but one in front of a set top bit
static u64 MacDerInteger(const u8* number, u8* out) {
    u64 start = 0;
    while (start < 31 && number[start] == 0) {
        start++;
    }

    bool padded = (number[start] & 0x80) != 0;
    u64 length = 32 - start + (padded ? 1 : 0);
    out[0] = 0x02;
    out[1] = (u8)length;
    u64 at = 2;
    if (padded) {
        out[at++] = 0;
    }

    memcpy(out + at, number + start, 32 - start);

    return 2 + length;
}

bool Platform::VerifyEcdsaP256(const u8* public_key, const u8* digest, const u8* signature) {
    // Security.framework takes the key as X9.63 (0x04, x, y) and the signature DER-encoded
    u8 key_bytes[65];
    key_bytes[0] = 0x04;
    memcpy(key_bytes + 1, public_key, 64);

    u8 integers[70];
    u64 integers_length = MacDerInteger(signature, integers);
    integers_length += MacDerInteger(signature + 32, integers + integers_length);
    u8 der[72];
    der[0] = 0x30;
    der[1] = (u8)integers_length;
    memcpy(der + 2, integers, integers_length);

    bool valid = false;
    @autoreleasepool {
        NSData* key_data = [NSData dataWithBytes:key_bytes length:sizeof(key_bytes)];
        NSDictionary* attributes = @{
            (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
            (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
            (id)kSecAttrKeySizeInBits: @256,
        };
        SecKeyRef key = SecKeyCreateWithData((CFDataRef)key_data, (CFDictionaryRef)attributes, nullptr);
        if (key) {
            NSData* digest_data = [NSData dataWithBytes:digest length:32];
            NSData* signature_data = [NSData dataWithBytes:der length:2 + integers_length];
            valid = SecKeyVerifySignature(key, kSecKeyAlgorithmECDSASignatureDigestX962SHA256, (CFDataRef)digest_data, (CFDataRef)signature_data, nullptr);
            CFRelease(key);
        }
    }

    return valid;
}

void MacTrustFailureText(__CFError* error, char* out, u64 capacity) {
    const char* known = nullptr;
    switch (error ? CFErrorGetCode(error) : 0) {
    case errSecNotTrusted:
    case errSecCreateChainFailed:       known = "signed by an untrusted authority (self-signed?)"; break;
    case errSecCertificateExpired:
    case errSecCertificateNotValidYet:  known = "expired or not yet valid"; break;
    case errSecHostNameMismatch:        known = "issued for a different host name"; break;
    case errSecCertificateRevoked:      known = "the certificate is revoked"; break;
    case errSecInvalidExtendedKeyUsage: known = "not valid for server authentication"; break;
    default:                            break;
    }

    if (known) {
        snprintf(out, capacity, "%s", known);
        return;
    }

    CFStringRef description = error ? CFErrorCopyDescription(error) : nullptr;
    if (!description || !CFStringGetCString(description, out, (CFIndex)capacity, kCFStringEncodingUTF8)) {
        snprintf(out, capacity, "the certificate could not be verified");
    }

    if (description) {
        CFRelease(description);
    }
}

u64 Platform::TimeNowNS() {
    return clock_gettime_nsec_np(CLOCK_REALTIME);
}

f64 Platform::GetClockTimeNS() {
    return (f64)clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

f64 Platform::GetAbsoluteTime() {
    return (f64)clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1000000000.0;
}

f64 Platform::GetElapsedTime() {
    return (f64)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - s_start_nanoseconds) / 1000000.0;
}

void Platform::SleepMilliseconds(u64 Milliseconds) {
    struct timespec duration = {(time_t)(Milliseconds / 1000), (long)((Milliseconds % 1000) * 1000000)};
    while (nanosleep(&duration, &duration) != 0 && errno == EINTR) {
    }
}

const char* Platform::GetKeyName(Keys key) {
#if defined(KRAFT_GUI_APP)
    int keycode = (int)key;
    return glfwGetKeyName(keycode, glfwGetKeyScancode(keycode));
#else
    return "";
#endif
}

const char* Platform::GetEnv(const char* Key) {
    return getenv(Key);
}

String8 Platform::ExecutablePath(ArenaAllocator* arena) {
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        return String8{};
    }

    // The path the program was started by can go through a symbolic link
    char resolved[PATH_MAX];
    const char* real = realpath(path, resolved) ? resolved : path;

    return StringCopy(arena, String8FromCString(real));
}

String8 Platform::ExecutableDirectory(ArenaAllocator* arena) {
    String8 path = ExecutablePath(arena);
    u64 length = path.count;
    while (length > 0 && path.ptr[length - 1] != '/') {
        length--;
    }

    if (length > 1) {
        length--;
    }

    return StringCopy(arena, String8{{path.ptr}, length});
}

u64 Platform::ProcessMemoryBytes() {
    task_vm_info_data_t info = {};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) {
        return 0;
    }

    // What Activity Monitor shows as the process's memory
    return info.phys_footprint;
}

String8 Platform::SystemFontDirectory(ArenaAllocator* arena) {
    return StringCopy(arena, S("/System/Library/Fonts"));
}

bool Platform::HasStandardOutput() {
    struct stat output = {};
    if (fstat(STDOUT_FILENO, &output) != 0) {
        return false;
    }

    // An app opened from Finder has its output thrown away into /dev/null
    struct stat null_device = {};
    return stat("/dev/null", &null_device) != 0 || output.st_rdev != null_device.st_rdev || output.st_ino != null_device.st_ino;
}

//
// Crash reports
//

#define KRAFT_CRASH_CONTEXT_MAX 2048
static char g_crash_context[KRAFT_CRASH_CONTEXT_MAX];

void Platform::SetCrashContext(const char* text) {
    if (!text) {
        g_crash_context[0] = 0;
        return;
    }

    strncpy(g_crash_context, text, sizeof(g_crash_context) - 1);
    g_crash_context[sizeof(g_crash_context) - 1] = 0;
}

static char g_crash_directory[PATH_MAX];

void Platform::SetCrashReportDirectory(const char* directory) {
    snprintf(g_crash_directory, sizeof(g_crash_directory), "%s", directory ? directory : "");
}

static void CrashReportDirectory(char* out, u64 capacity) {
    if (g_crash_directory[0]) {
        snprintf(out, capacity, "%s/crashes", g_crash_directory);
        return;
    }

    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        path[0] = 0;
    }

    u64 length = strlen(path);
    while (length > 0 && path[length - 1] != '/') {
        length--;
    }

    snprintf(out, capacity, "%.*scrashes", (int)length, path);
}

u32 Platform::CrashReportCount() {
    char directory[PATH_MAX];
    CrashReportDirectory(directory, sizeof(directory));

    DIR* listing = opendir(directory);
    if (!listing) {
        return 0;
    }

    u32 count = 0;
    for (struct dirent* entry = readdir(listing); entry; entry = readdir(listing)) {
        u64 length = strlen(entry->d_name);
        if (strncmp(entry->d_name, "pigeon-crash-", 13) == 0 && length > 4 && strcmp(entry->d_name + length - 4, ".txt") == 0) {
            count++;
        }
    }
    closedir(listing);

    return count;
}

static int g_crash_file = -1;

static void CrashPrint(const char* format, ...) {
    char line[1024];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length <= 0) {
        return;
    }

    if (length >= (int)sizeof(line)) {
        length = (int)sizeof(line) - 1;
    }

    write(STDERR_FILENO, line, (size_t)length);
    if (g_crash_file >= 0) {
        write(g_crash_file, line, (size_t)length);
    }
}

static const char* CrashSignalName(int signal_number) {
    switch (signal_number) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS:  return "SIGBUS";
    case SIGILL:  return "SIGILL";
    case SIGFPE:  return "SIGFPE";
    case SIGABRT: return "SIGABRT";
    case SIGTRAP: return "SIGTRAP";
    }

    return "signal";
}

static const int g_crash_signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};

static void CrashHandler(int signal_number, siginfo_t* info, void* context) {
    // Back to the default first, so a crash in here, or the signal raised again at the end, ends the process
    for (u32 index = 0; index < KRAFT_C_ARRAY_SIZE(g_crash_signals); index++) {
        signal(g_crash_signals[index], SIG_DFL);
    }

    char directory[PATH_MAX];
    CrashReportDirectory(directory, sizeof(directory));
    mkdir(directory, 0755);

    time_t now_seconds = time(nullptr);
    struct tm now = {};
    localtime_r(&now_seconds, &now);
    char report_path[PATH_MAX];
    snprintf(report_path, sizeof(report_path), "%s/pigeon-crash-%04d%02d%02d-%02d%02d%02d.txt", directory, now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
    g_crash_file = open(report_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    fprintf(stderr, "\n[crash] writing %s\n", report_path);
    if (g_crash_file >= 0) {
        char header[256];
        int length = snprintf(header, sizeof(header), "pigeon crash report %04d-%02d-%02d %02d:%02d:%02d\n", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
        write(g_crash_file, header, (size_t)length);
        if (g_crash_context[0]) {
            write(g_crash_file, g_crash_context, strlen(g_crash_context));
            write(g_crash_file, "\n", 1);
        }
    }

    ucontext_t* user_context = (ucontext_t*)context;
#if defined(__arm64__)
    u64 pc = user_context ? (u64)__darwin_arm_thread_state64_get_pc(user_context->uc_mcontext->__ss) : 0;
    u64 sp = user_context ? (u64)__darwin_arm_thread_state64_get_sp(user_context->uc_mcontext->__ss) : 0;
#else
    u64 pc = user_context ? (u64)user_context->uc_mcontext->__ss.__rip : 0;
    u64 sp = user_context ? (u64)user_context->uc_mcontext->__ss.__rsp : 0;
#endif
    u64 load_address = (u64)_dyld_get_image_header(0);
    CrashPrint("[crash] %s (%d) at 0x%llX, code %d, address 0x%llX\n", CrashSignalName(signal_number), signal_number, (unsigned long long)pc, info ? info->si_code : 0,
               (unsigned long long)(info ? (u64)info->si_addr : 0));
    CrashPrint("[crash] pc offset=0x%llX sp=0x%llX load address=0x%llX\n", (unsigned long long)(pc - load_address), (unsigned long long)sp, (unsigned long long)load_address);

    void* frames[64];
    int frame_count = backtrace(frames, (int)KRAFT_C_ARRAY_SIZE(frames));
    backtrace_symbols_fd(frames, frame_count, STDERR_FILENO);
    if (g_crash_file >= 0) {
        backtrace_symbols_fd(frames, frame_count, g_crash_file);
    }

    // The executable's own functions have no names here; atos finds them in the dSYM kept from the build, the
    // instruction that faulted first
    CrashPrint("\n[crash] to symbolicate: atos -o pigeon.dSYM -l 0x%llX 0x%llX", (unsigned long long)load_address, (unsigned long long)pc);
    for (int index = 0; index < frame_count; index++) {
        CrashPrint(" 0x%llX", (unsigned long long)(u64)frames[index]);
    }
    CrashPrint("\n");

    if (g_crash_file >= 0 && LogHistoryCount() > 0) {
        CrashPrint("\n[crash] last %u log lines:\n", LogHistoryCount());
        for (u32 index = 0; index < LogHistoryCount(); index++) {
            const char* line = LogHistoryLine(index);
            write(g_crash_file, "  ", 2);
            write(g_crash_file, line, strlen(line));
            write(g_crash_file, "\n", 1);
        }
    }

    if (g_crash_file >= 0) {
        close(g_crash_file);
        g_crash_file = -1;
    }

    raise(signal_number);
}

void Platform::InstallCrashHandler() {
    // A stack of its own, so a stack overflow can still be reported
    static u8 alternate_stack[KRAFT_SIZE_KB(256)];
    stack_t stack = {};
    stack.ss_sp = alternate_stack;
    stack.ss_size = sizeof(alternate_stack);
    sigaltstack(&stack, nullptr);

    // The first call loads what backtrace needs, which is not safe to do inside a signal handler
    void* frames[4];
    backtrace(frames, 4);

    struct sigaction action = {};
    action.sa_sigaction = CrashHandler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    for (u32 index = 0; index < KRAFT_C_ARRAY_SIZE(g_crash_signals); index++) {
        sigaction(g_crash_signals[index], &action, nullptr);
    }
}

#if defined(KRAFT_GUI_APP)

// GLFW reports a scroll from inside the scroll event's handler, so it is still the event being handled
bool MacScrollIsPrecise() {
    NSEvent* event = [NSApp currentEvent];

    return event && event.type == NSEventTypeScrollWheel && event.hasPreciseScrollingDeltas;
}

Window* Platform::CreatePlatformWindow(const WindowOptions* Opts) {
    State->PrimaryWindow = (Window*)Malloc(sizeof(Window), false);
    if (State->PrimaryWindow->Init(Opts) != 0) {
        Free(State->PrimaryWindow);
        State->PrimaryWindow = nullptr;
        return nullptr;
    }

    State->PrimaryWindow->PlatformWindowState = (PlatformWindowState*)Malloc(sizeof(PlatformWindowState), false);
    MemSet(State->PrimaryWindow->PlatformWindowState, 0, sizeof(PlatformWindowState));
    State->PrimaryWindow->PlatformWindowState->ns_window = (void*)glfwGetCocoaWindow(State->PrimaryWindow->PlatformWindowHandle);

    return State->PrimaryWindow;
}

void Platform::DestroyPlatformWindow(Window* window) {
    State->PrimaryWindow->Destroy();

    Free(State->PrimaryWindow->PlatformWindowState);
    Free(State->PrimaryWindow);
    State->PrimaryWindow = nullptr;
}

static NSString* MacString(String8 text) {
    return [[[NSString alloc] initWithBytes:text.ptr length:text.count encoding:NSUTF8StringEncoding] autorelease];
}

static String8 MacPushString(ArenaAllocator* arena, NSString* text) {
    const char* utf8 = [text UTF8String];

    return StringCopy(arena, String8FromCString(utf8 ? utf8 : ""));
}

// The file types a "Name|*.ext;*.ext|Name|*.*" filter names, one list of UTType per entry; an empty list allows any file
static NSArray* MacFilterTypes(const char* filter, NSMutableArray* out_names) {
    NSMutableArray* type_lists = [NSMutableArray array];
    if (!filter || !filter[0]) {
        return type_lists;
    }

    NSArray* parts = [[NSString stringWithUTF8String:filter] componentsSeparatedByString:@"|"];
    for (NSUInteger index = 0; index + 1 < parts.count; index += 2) {
        NSMutableArray* types = [NSMutableArray array];
        for (NSString* pattern in [parts[index + 1] componentsSeparatedByString:@";"]) {
            NSString* trimmed = [pattern stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            if (![trimmed hasPrefix:@"*."] || [trimmed isEqualToString:@"*.*"]) {
                continue;
            }

            UTType* type = [UTType typeWithFilenameExtension:[trimmed substringFromIndex:2]];
            if (type) {
                [types addObject:type];
            }
        }

        [type_lists addObject:types];
        [out_names addObject:parts[index]];
    }

    return type_lists;
}

} // namespace kraft

// The popup under a file dialog that switches between a filter's entries, as the type list in Windows' dialog does
@interface KraftDialogFilter : NSObject
@property(assign) NSSavePanel* panel;
@property(retain) NSArray* typeLists;
- (void)filterChanged:(NSPopUpButton*)sender;
@end

@implementation KraftDialogFilter
- (void)filterChanged:(NSPopUpButton*)sender {
    NSInteger index = sender.indexOfSelectedItem;
    if (index >= 0 && (NSUInteger)index < self.typeLists.count) {
        self.panel.allowedContentTypes = self.typeLists[(NSUInteger)index];
    }
}

- (void)dealloc {
    [_typeLists release];
    [super dealloc];
}
@end

namespace kraft {

// Gives a dialog the filter's first entry, and the popup to choose another when there is more than one
static KraftDialogFilter* MacDialogApplyFilter(NSSavePanel* panel, const char* filter) {
    NSMutableArray* names = [NSMutableArray array];
    NSArray* type_lists = MacFilterTypes(filter, names);
    if (type_lists.count == 0) {
        return nil;
    }

    panel.allowedContentTypes = type_lists[0];
    if (type_lists.count < 2) {
        return nil;
    }

    KraftDialogFilter* chooser = [[[KraftDialogFilter alloc] init] autorelease];
    chooser.panel = panel;
    chooser.typeLists = type_lists;

    NSPopUpButton* popup = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 220, 26) pullsDown:NO] autorelease];
    [popup addItemsWithTitles:names];
    popup.target = chooser;
    popup.action = @selector(filterChanged:);

    NSTextField* label = [NSTextField labelWithString:@"Show:"];
    NSStackView* row = [NSStackView stackViewWithViews:@[label, popup]];
    row.edgeInsets = NSEdgeInsetsMake(8, 12, 8, 12);
    panel.accessoryView = row;
    if ([panel isKindOfClass:[NSOpenPanel class]]) {
        ((NSOpenPanel*)panel).accessoryViewDisclosed = YES;
    }

    return chooser;
}

bool Platform::OpenFolderDialog(ArenaAllocator* arena, const char* title, String8* out_path) {
    bool ok = false;
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = NO;
        panel.canChooseDirectories = YES;
        panel.canCreateDirectories = YES;
        panel.allowsMultipleSelection = NO;
        if (title) {
            // A Mac's panels show the message, not the title
            panel.message = [NSString stringWithUTF8String:title];
        }

        if ([panel runModal] == NSModalResponseOK && panel.URL) {
            *out_path = MacPushString(arena, panel.URL.path);
            ok = true;
        }
    }

    return ok;
}

bool Platform::OpenFileDialog(ArenaAllocator* arena, const char* filter, String8* out_path) {
    bool ok = false;
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        KraftDialogFilter* chooser = MacDialogApplyFilter(panel, filter);
        if ([panel runModal] == NSModalResponseOK && panel.URL) {
            *out_path = MacPushString(arena, panel.URL.path);
            ok = true;
        }
        (void)chooser;
    }

    return ok;
}

bool Platform::SaveFileDialog(ArenaAllocator* arena, const char* filter, const char* default_extension, String8* out_path, String8 suggested_name) {
    bool ok = false;
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        panel.canCreateDirectories = YES;
        panel.allowsOtherFileTypes = YES;
        KraftDialogFilter* chooser = MacDialogApplyFilter(panel, filter);
        if (default_extension && default_extension[0] && panel.allowedContentTypes.count == 0) {
            UTType* type = [UTType typeWithFilenameExtension:[NSString stringWithUTF8String:default_extension]];
            if (type) {
                panel.allowedContentTypes = @[type];
            }
        }

        if (suggested_name.count > 0) {
            panel.nameFieldStringValue = MacString(suggested_name);
        }

        if ([panel runModal] == NSModalResponseOK && panel.URL) {
            *out_path = MacPushString(arena, panel.URL.path);
            ok = true;
        }
        (void)chooser;
    }

    return ok;
}

bool Platform::OpenWithSystem(String8 path) {
    bool opened = false;
    @autoreleasepool {
        NSString* text = MacString(path);
        NSURL* url = [text rangeOfString:@"://"].location != NSNotFound ? [NSURL URLWithString:text] : [NSURL fileURLWithPath:text];
        opened = url && [[NSWorkspace sharedWorkspace] openURL:url];
    }

    return opened;
}

bool Platform::RevealInFileManager(String8 path) {
    bool revealed = false;
    @autoreleasepool {
        NSString* text = MacString(path);
        if ([[NSFileManager defaultManager] fileExistsAtPath:text]) {
            [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[[NSURL fileURLWithPath:text]]];
            revealed = true;
        }
    }

    return revealed;
}

void Platform::ShowErrorDialog(const char* title, String8 message) {
    @autoreleasepool {
        // Before the window is up there may be no application object yet, nor a place in the Dock to show from
        [NSApplication sharedApplication];
        if (NSApp.activationPolicy == NSApplicationActivationPolicyProhibited) {
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        }

        NSAlert* alert = [[[NSAlert alloc] init] autorelease];
        alert.alertStyle = NSAlertStyleCritical;
        alert.messageText = [NSString stringWithUTF8String:title ? title : ""];
        alert.informativeText = MacString(message);
        [alert addButtonWithTitle:@"OK"];
        [NSApp activateIgnoringOtherApps:YES];
        [alert runModal];
    }
}

#endif

} // namespace kraft

#endif
