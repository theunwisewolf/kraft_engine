#include <net/kraft_http.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_threading.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winhttp.h>

#include <core/kraft_core_includes.h>

#pragma comment(lib, "winhttp")

namespace kraft::net {

struct HttpTask {
    Thread thread;
    volatile i32 done;
    void* volatile connection;
    void* volatile request_handle;
    ArenaAllocator* arena;
    HttpRequest request;
    HttpResponse response;
    DWORD secure_failure_flags;
    b32 tls_verification_skipped;
    volatile i32 cancelled;
    volatile i32 timed_out;
    f64 deadline;
};

// WinHTTP keeps connections alive per session, so one session for the app's lifetime
static HINTERNET g_http_session;
static wchar_t g_http_user_agent[128] = L"kraft";

static HINTERNET HttpSessionGet() {
    if (!g_http_session) {
        g_http_session = WinHttpOpen(g_http_user_agent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (g_http_session) {
            DWORD http2 = WINHTTP_PROTOCOL_FLAG_HTTP2;
            WinHttpSetOption(g_http_session, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &http2, sizeof(http2));
        }
    }

    return g_http_session;
}

void HttpSetUserAgent(String8 user_agent) {
    i32 count = MultiByteToWideChar(CP_UTF8, 0, user_agent.str, (i32)user_agent.count, g_http_user_agent, (i32)KRAFT_C_ARRAY_SIZE(g_http_user_agent) - 1);
    g_http_user_agent[count > 0 ? count : 0] = 0;
}

void HttpShutdown() {
    if (g_http_session) {
        WinHttpCloseHandle(g_http_session);
        g_http_session = nullptr;
    }
}

static void HttpCloseSwap(void* volatile* slot) {
    void* handle = AtomicExchangePointer(slot, nullptr);
    if (handle) {
        WinHttpCloseHandle((HINTERNET)handle);
    }
}

const char* HttpMethodName(HttpMethod method) {
    switch (method) {
    case HTTP_METHOD_GET:    return "GET";
    case HTTP_METHOD_POST:   return "POST";
    case HTTP_METHOD_PUT:    return "PUT";
    case HTTP_METHOD_PATCH:  return "PATCH";
    case HTTP_METHOD_DELETE: return "DELETE";
    case HTTP_METHOD_HEAD:   return "HEAD";
    }

    return "GET";
}

static const wchar_t* HttpMethodWide(HttpMethod method) {
    switch (method) {
    case HTTP_METHOD_GET:    return L"GET";
    case HTTP_METHOD_POST:   return L"POST";
    case HTTP_METHOD_PUT:    return L"PUT";
    case HTTP_METHOD_PATCH:  return L"PATCH";
    case HTTP_METHOD_DELETE: return L"DELETE";
    case HTTP_METHOD_HEAD:   return L"HEAD";
    }

    return L"GET";
}

static wchar_t* HttpPushWide(ArenaAllocator* arena, String8 text) {
    int count = MultiByteToWideChar(CP_UTF8, 0, text.str, (int)text.count, nullptr, 0);
    wchar_t* wide = ArenaPushArrayNoZero(arena, wchar_t, (u64)count + 1);
    MultiByteToWideChar(CP_UTF8, 0, text.str, (int)text.count, wide, count);
    wide[count] = 0;

    return wide;
}

static String8 HttpPushUtf8(ArenaAllocator* arena, const wchar_t* wide, int wide_count) {
    int count = WideCharToMultiByte(CP_UTF8, 0, wide, wide_count, nullptr, 0, nullptr, nullptr);

    String8 text;
    text.ptr = ArenaPushArrayNoZero(arena, u8, (u64)count + 1);
    WideCharToMultiByte(CP_UTF8, 0, wide, wide_count, text.str, count, nullptr, nullptr);
    if (wide_count < 0 && count > 0) {
        count -= 1;
    }

    text.count = (u64)count;
    text.str[text.count] = 0;

    return text;
}

static void HttpPushBytes(ArenaAllocator* arena, String8 text) {
    MemCpy(arena->Push(text.count, 1, false), text.ptr, text.count);
}

static u64 HttpPushWithoutLineBreaks(ArenaAllocator* arena, String8 text) {
    u64 count = 0;
    for (u64 index = 0; index < text.count; index++) {
        u8 character = text.ptr[index];
        if (character == '\r' || character == '\n' || character == 0) {
            continue;
        }

        *(u8*)arena->Push(1, 1, false) = character;
        count++;
    }

    return count;
}

static String8 HttpCopyWithoutLineBreaks(ArenaAllocator* arena, String8 text) {
    u8* start = (u8*)arena->Push(0, 1, false);
    u64 count = HttpPushWithoutLineBreaks(arena, text);

    return String8{{start}, count};
}

static b32 HttpEqualsIgnoringCase(String8 text, const char* other) {
    for (u64 index = 0; index < text.count; index++) {
        u8 left = text.ptr[index];
        u8 right = (u8)other[index];
        if (right == 0) {
            return false;
        }
        if (left >= 'A' && left <= 'Z') {
            left += 32;
        }
        if (right >= 'A' && right <= 'Z') {
            right += 32;
        }
        if (left != right) {
            return false;
        }
    }

    return other[text.count] == 0;
}

static String8 HttpErrorText(ArenaAllocator* arena, DWORD code) {
    const char* known = nullptr;
    switch (code) {
    case ERROR_WINHTTP_TIMEOUT:                  known = "timed out"; break;
    case ERROR_WINHTTP_INTERNAL_ERROR:           known = "WinHTTP internal error"; break;
    case ERROR_WINHTTP_INVALID_URL:              known = "the URL is not valid"; break;
    case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:      known = "the URL scheme is not http or https"; break;
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:        known = "the host name could not be resolved"; break;
    case ERROR_WINHTTP_CANNOT_CONNECT:           known = "could not connect: nothing is listening, or a firewall refused it"; break;
    case ERROR_WINHTTP_CONNECTION_ERROR:         known = "the connection was reset"; break;
    case ERROR_WINHTTP_OPERATION_CANCELLED:      known = "cancelled"; break;
    case ERROR_WINHTTP_LOGIN_FAILURE:            known = "authentication was rejected"; break;
    case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED:  known = "the server asked for a client certificate"; break;
    case ERROR_WINHTTP_INVALID_SERVER_RESPONSE:  known = "the reply was not valid HTTP"; break;
    case ERROR_WINHTTP_REDIRECT_FAILED:          known = "too many redirects, or one that could not be followed"; break;
    case ERROR_WINHTTP_SECURE_CHANNEL_ERROR:     known = "the TLS handshake failed"; break;
    case ERROR_WINHTTP_SECURE_FAILURE:           known = "the server's TLS certificate could not be verified"; break;
    case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID: known = "the server's certificate has expired or is not valid yet"; break;
    case ERROR_WINHTTP_SECURE_CERT_CN_INVALID:   known = "the certificate was issued for a different host name"; break;
    case ERROR_WINHTTP_SECURE_INVALID_CA:        known = "the certificate was signed by an authority this machine does not trust"; break;
    case ERROR_WINHTTP_SECURE_CERT_REV_FAILED:   known = "the certificate's revocation status could not be checked"; break;
    case ERROR_WINHTTP_SECURE_CERT_REVOKED:      known = "the server's certificate has been revoked"; break;
    case ERROR_WINHTTP_SECURE_INVALID_CERT:      known = "the server's certificate is not valid"; break;
    case ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE:  known = "the certificate is not valid for server authentication"; break;
    default:                                     break;
    }

    if (known) {
        return StringFormat(arena, "%s (win32 %lu)", known, code);
    }

    wchar_t* text = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS;
    HMODULE module = (code >= WINHTTP_ERROR_BASE && code <= WINHTTP_ERROR_LAST) ? GetModuleHandleW(L"winhttp.dll") : nullptr;
    flags |= module ? FORMAT_MESSAGE_FROM_HMODULE : FORMAT_MESSAGE_FROM_SYSTEM;
    DWORD length = FormatMessageW(flags, module, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&text, 0, nullptr);
    if (length == 0 || !text) {
        return StringFormat(arena, "win32 error %lu", code);
    }

    while (length > 0 && (text[length - 1] == L'\n' || text[length - 1] == L'\r' || text[length - 1] == L'.' || text[length - 1] == L' ')) {
        text[--length] = 0;
    }

    i32 bytes = WideCharToMultiByte(CP_UTF8, 0, text, (i32)length, nullptr, 0, nullptr, nullptr);
    String8 message = {};
    if (bytes > 0) {
        message.ptr = ArenaPushArrayNoZero(arena, u8, (u64)bytes + 1);
        WideCharToMultiByte(CP_UTF8, 0, text, (i32)length, (char*)message.ptr, bytes, nullptr, nullptr);
        message.count = (u64)bytes;
        message.ptr[bytes] = 0;
    }
    LocalFree(text);

    if (message.count == 0) {
        return StringFormat(arena, "win32 error %lu", code);
    }

    return StringFormat(arena, "%S (win32 %lu)", message, code);
}

static String8 HttpSecureFailureText(ArenaAllocator* arena, DWORD flags) {
    if (flags == 0) {
        return S("the certificate could not be verified");
    }

    String8 result = {};
    struct {
        DWORD flag;
        const char* text;
    } reasons[] = {
        {WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED, "revocation could not be checked"},
        {WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT, "the certificate is invalid"},
        {WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED, "the certificate is revoked"},
        {WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA, "signed by an untrusted authority"},
        {WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID, "issued for a different host name"},
        {WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID, "expired or not yet valid"},
        {WINHTTP_CALLBACK_STATUS_FLAG_CERT_WRONG_USAGE, "not valid for server authentication"},
        {WINHTTP_CALLBACK_STATUS_FLAG_SECURITY_CHANNEL_ERROR, "the secure channel failed"},
    };

    for (u32 index = 0; index < KRAFT_C_ARRAY_SIZE(reasons); index++) {
        if (!(flags & reasons[index].flag)) {
            continue;
        }

        result = result.count > 0 ? StringFormat(arena, "%S, %s", result, reasons[index].text) : StringFormat(arena, "%s", reasons[index].text);
    }

    return result.count > 0 ? result : S("the certificate could not be verified");
}

// Splits "Key: value" lines after the status line into headers that point into the text
static void HttpParseHeaders(ArenaAllocator* arena, String8 raw_headers, HttpHeader** out_headers, u32* out_count) {
    u32 max_lines = 1;
    for (u64 index = 0; index < raw_headers.count; index++) {
        if (raw_headers.ptr[index] == '\n') {
            max_lines++;
        }
    }

    HttpHeader* parsed = ArenaPushArray(arena, HttpHeader, max_lines);
    u32 parsed_count = 0;
    b32 status_line = true;
    u64 position = 0;
    while (position < raw_headers.count) {
        u64 line_end = position;
        while (line_end < raw_headers.count && raw_headers.ptr[line_end] != '\n') {
            line_end++;
        }

        u64 end = line_end;
        if (end > position && raw_headers.ptr[end - 1] == '\r') {
            end--;
        }

        if (status_line) {
            status_line = false;
        } else if (end > position) {
            u64 colon = position;
            while (colon < end && raw_headers.ptr[colon] != ':') {
                colon++;
            }

            u64 value_start = colon < end ? colon + 1 : end;
            while (value_start < end && raw_headers.ptr[value_start] == ' ') {
                value_start++;
            }

            parsed[parsed_count].key = String8{{raw_headers.ptr + position}, colon - position};
            parsed[parsed_count].value = String8{{raw_headers.ptr + value_start}, end - value_start};
            parsed_count++;
        }

        position = line_end + 1;
    }

    *out_headers = parsed;
    *out_count = parsed_count;
}

static String8 HttpQueryRawHeaders(ArenaAllocator* arena, HINTERNET request) {
    DWORD raw_length = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &raw_length, WINHTTP_NO_HEADER_INDEX);
    if (raw_length == 0) {
        return String8{};
    }

    wchar_t* raw = ArenaPushArrayNoZero(arena, wchar_t, raw_length / 2 + 1);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw, &raw_length, WINHTTP_NO_HEADER_INDEX)) {
        return String8{};
    }

    return HttpPushUtf8(arena, raw, -1);
}

// WinHTTP follows the redirect itself, with the headers left on the request handle once this returns
static void HttpRedirected(HttpTask* task, HINTERNET request, const wchar_t* next_url) {
    ArenaAllocator* arena = task->arena;
    HttpRedirect redirect = {};
    redirect.arena = arena;
    redirect.url = HttpPushUtf8(arena, next_url, -1);

    DWORD code = 0;
    DWORD code_size = sizeof(code);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &code_size, WINHTTP_NO_HEADER_INDEX);
    redirect.status = (i32)code;
    HttpParseHeaders(arena, HttpQueryRawHeaders(arena, request), &redirect.headers, &redirect.header_count);
    task->request.before_redirect(&redirect, task->request.before_redirect_user);

    for (u32 index = 0; index < redirect.next_header_count; index++) {
        String8 key = HttpCopyWithoutLineBreaks(arena, redirect.next_headers[index].key);
        String8 value = HttpCopyWithoutLineBreaks(arena, redirect.next_headers[index].value);
        if (key.count == 0) {
            continue;
        }

        // Removes every header of this name, one per call, then adds the new one unless its value is empty
        wchar_t* removal = HttpPushWide(arena, StringFormat(arena, "%S:", key));
        for (u32 removed = 0; removed < 64 && WinHttpAddRequestHeaders(request, removal, (DWORD)-1, WINHTTP_ADDREQ_FLAG_REPLACE); removed++) {}

        if (value.count > 0) {
            WinHttpAddRequestHeaders(request, HttpPushWide(arena, StringFormat(arena, "%S: %S", key, value)), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
        }
    }
}

static void CALLBACK HttpStatusCallback(HINTERNET handle, DWORD_PTR context, DWORD status, void* info, DWORD info_length) {
    if (context == 0) {
        return;
    }

    HttpTask* task = (HttpTask*)context;
    if (status == WINHTTP_CALLBACK_STATUS_SECURE_FAILURE) {
        task->secure_failure_flags = (info && info_length >= sizeof(DWORD)) ? *(DWORD*)info : 0;
    } else if (status == WINHTTP_CALLBACK_STATUS_REDIRECT && info && task->request.before_redirect) {
        HttpRedirected(task, handle, (const wchar_t*)info);
    }
}

// Windows before 10 2004 has no per-step times, so the request is only measured as a whole
static void HttpReadTimings(HINTERNET request, f64 headers_time, f64 end_time, NetTimings* out) {
    WINHTTP_REQUEST_TIMES times = {};

    // In: the room there is, out: the count; 0 is rejected
    times.cTimes = WinHttpRequestTimeMax;
    DWORD size = sizeof(times);
    if (!WinHttpQueryOption(request, WINHTTP_OPTION_REQUEST_TIMES, &times, &size)) {
        return;
    }

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    // A reused connection still carries the DNS, TCP and TLS times from when it was opened
    u64 asked_at = WinHttpConnectionAcquireStart < times.cTimes ? times.rgullTimes[WinHttpConnectionAcquireStart] : 0;
    auto span = [&](u32 from, u32 to) -> u64 {
        if (from >= times.cTimes || to >= times.cTimes || times.rgullTimes[from] == 0 || times.rgullTimes[from] < asked_at || times.rgullTimes[to] <= times.rgullTimes[from]) {
            return 0;
        }
        return (u64)((times.rgullTimes[to] - times.rgullTimes[from]) * 1000000 / (u64)frequency.QuadPart);
    };

    out->measured = true;
    out->dns_micros = span(WinHttpNameResolutionStart, WinHttpNameResolutionEnd);
    out->connect_micros = span(WinHttpConnectionEstablishmentStart, WinHttpConnectionEstablishmentEnd);

    // Up to the connection being handed over, so checking the certificate counts too
    out->tls_micros = span(WinHttpTlsHandshakeClientLeg1Start, WinHttpConnectionAcquireEnd);
    out->send_micros = span(WinHttpSendRequestStart, WinHttpSendRequestEnd);
    out->wait_micros = span(WinHttpSendRequestEnd, WinHttpReceiveResponseHeadersEnd);
    out->receive_micros = headers_time > 0.0 && end_time > headers_time ? (u64)((end_time - headers_time) * 1000000.0) : 0;

    // Not WINHTTP_REQUEST_STAT_FLAG_FIRST_REQUEST, which HTTP/2 sets on every request
    u64 connect_started = WinHttpConnectionEstablishmentStart < times.cTimes ? times.rgullTimes[WinHttpConnectionEstablishmentStart] : 0;
    out->reused = connect_started == 0 || connect_started < asked_at;
}

static void HttpFail(HttpTask* task, const char* reason) {
    DWORD code = GetLastError();
    task->response.ok = false;
    task->response.error = StringFormat(task->arena, "%s: %S", reason, HttpErrorText(task->arena, code));
}

static KRAFT_THREAD_ENTRY(HttpThreadProc) {
    HttpTask* task = (HttpTask*)user_data;
    ArenaAllocator* arena = task->arena;
    HttpResponse* response = &task->response;
    *response = {};

    f64 start_time = Platform::GetAbsoluteTime();

    HINTERNET session = HttpSessionGet();
    HINTERNET connection = nullptr;
    HINTERNET request = nullptr;

    wchar_t* wide_url = HttpPushWide(arena, task->request.url);

    wchar_t host[256] = {};
    wchar_t path[4096] = {};
    wchar_t extra[4096] = {};
    URL_COMPONENTS url_parts = {};
    url_parts.dwStructSize = sizeof(url_parts);
    url_parts.lpszHostName = host;
    url_parts.dwHostNameLength = ARRAYSIZE(host);
    url_parts.lpszUrlPath = path;
    url_parts.dwUrlPathLength = ARRAYSIZE(path);
    url_parts.lpszExtraInfo = extra;
    url_parts.dwExtraInfoLength = ARRAYSIZE(extra);

    if (!WinHttpCrackUrl(wide_url, 0, 0, &url_parts)) {
        HttpFail(task, "invalid URL");
        goto done;
    }

    {
        wchar_t object[8192];
        lstrcpynW(object, path, ARRAYSIZE(object));
        lstrcatW(object, extra);

        if (!session) {
            HttpFail(task, "WinHttpOpen failed");
            goto done;
        }

        connection = WinHttpConnect(session, host, url_parts.nPort, 0);
        if (!connection) {
            HttpFail(task, "connect failed");
            goto done;
        }
        task->connection = connection;

        DWORD flags = (url_parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        request = WinHttpOpenRequest(connection, HttpMethodWide(task->request.method), object, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!request) {
            HttpFail(task, "open request failed");
            goto done;
        }
        task->request_handle = request;
        DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
        WinHttpSetOption(request, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));
        DWORD no_cookies = WINHTTP_DISABLE_COOKIES;
        WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &no_cookies, sizeof(no_cookies));
        if (task->request.no_redirects) {
            DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
        } else if (task->request.max_redirects > 0) {
            DWORD max_redirects = task->request.max_redirects;
            WinHttpSetOption(request, WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS, &max_redirects, sizeof(max_redirects));
        }

        // The only way to learn why a certificate was rejected, a redirect's too, and what a redirect said
        {
            DWORD_PTR context = (DWORD_PTR)task;
            DWORD callbacks = WINHTTP_CALLBACK_FLAG_SECURE_FAILURE | (task->request.before_redirect ? WINHTTP_CALLBACK_FLAG_REDIRECT : 0);
            WinHttpSetOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context));
            WinHttpSetStatusCallback(request, HttpStatusCallback, callbacks, 0);
        }

        u8* headers_start = (u8*)arena->Push(0, 1, false);
        b32 has_content_type = false;
        for (u32 index = 0; index < task->request.header_count; index++) {
            HttpHeader header = task->request.headers[index];
            u8* key_start = (u8*)arena->Push(0, 1, false);
            u64 key_count = HttpPushWithoutLineBreaks(arena, header.key);
            if (key_count == 0) {
                continue;
            }
            if (HttpEqualsIgnoringCase(String8{{key_start}, key_count}, "Content-Type")) {
                has_content_type = true;
            }
            HttpPushBytes(arena, S(": "));
            HttpPushWithoutLineBreaks(arena, header.value);
            HttpPushBytes(arena, S("\r\n"));
        }

        if (task->request.body.count > 0 && !has_content_type) {
            HttpPushBytes(arena, S("Content-Type: application/json\r\n"));
        }

        u64 headers_count = (u64)((u8*)arena->Push(0, 1, false) - headers_start);

        const wchar_t* headers = WINHTTP_NO_ADDITIONAL_HEADERS;
        DWORD headers_length = 0;
        if (headers_count > 0) {
            headers = HttpPushWide(arena, String8{{headers_start}, headers_count});
            headers_length = (DWORD)-1;
        }

        void* body = WINHTTP_NO_REQUEST_DATA;
        DWORD body_length = 0;
        if (task->request.body.count > 0) {
            body = task->request.body.str;
            body_length = (DWORD)task->request.body.count;
        }

        if (!WinHttpSendRequest(request, headers, headers_length, body, body_length, body_length, 0)) {
            DWORD send_error = GetLastError();
            if (send_error != ERROR_WINHTTP_SECURE_FAILURE) {
                SetLastError(send_error);
                HttpFail(task, "send failed");
                goto done;
            }

            // Retried with verification off, and reported in tls_warning
            DWORD ignore = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            if (!WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &ignore, sizeof(ignore))) {
                SetLastError(send_error);
                HttpFail(task, "send failed");
                goto done;
            }

            if (!WinHttpSendRequest(request, headers, headers_length, body, body_length, body_length, 0)) {
                HttpFail(task, "send failed");
                goto done;
            }
            task->tls_verification_skipped = true;
        }

        if (!WinHttpReceiveResponse(request, nullptr)) {
            HttpFail(task, "receive failed");
            goto done;
        }

        f64 headers_time = Platform::GetAbsoluteTime();

        if (task->tls_verification_skipped) {
            response->tls_warning = StringFormat(arena, "TLS not verified: %S", HttpSecureFailureText(arena, task->secure_failure_flags));
        }

        DWORD code = 0;
        DWORD code_size = sizeof(code);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &code_size, WINHTTP_NO_HEADER_INDEX);
        response->status = (i32)code;

        DWORD text_length = 0;
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_TEXT, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &text_length, WINHTTP_NO_HEADER_INDEX);
        if (text_length > 0) {
            wchar_t* status_text = ArenaPushArrayNoZero(arena, wchar_t, text_length / 2 + 1);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_TEXT, WINHTTP_HEADER_NAME_BY_INDEX, status_text, &text_length, WINHTTP_NO_HEADER_INDEX)) {
                response->status_text = HttpPushUtf8(arena, status_text, -1);
            }
        }

        response->raw_headers = HttpQueryRawHeaders(arena, request);
        if (response->raw_headers.count > 0) {
            HttpParseHeaders(arena, response->raw_headers, &response->headers, &response->header_count);
        }

        u8* body_start = (u8*)arena->Push(0, 1, false);
        u64 total = 0;
        b32 body_error = false;
        b32 too_large = false;
        const char* body_message = nullptr;
        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available)) {
                body_error = true;
                body_message = "body query failed";
                break;
            }

            if (available == 0) {
                break;
            }

            while (available > 0) {
                BYTE chunk[16384];
                DWORD to_read = available < sizeof(chunk) ? available : sizeof(chunk);
                DWORD read = 0;
                if (!WinHttpReadData(request, chunk, to_read, &read)) {
                    body_error = true;
                    body_message = "body read failed";
                    available = 0;
                    break;
                }

                if (read == 0) {
                    available = 0;
                    break;
                }

                if (total + read > KRAFT_HTTP_MAX_BODY || arena->position + read + 1 > arena->capacity) {
                    body_error = true;
                    too_large = true;
                    available = 0;
                    break;
                }

                u8* destination = (u8*)arena->Push(read, 1, false);
                MemCpy(destination, chunk, read);
                total += read;
                available -= read;
            }

            if (body_error) {
                break;
            }
        }

        HttpReadTimings(request, headers_time, Platform::GetAbsoluteTime(), &response->timings);
        if (too_large) {
            response->ok = false;
            response->error = StringFormat(arena, "the response is larger than %llu MB, the most that is read", (unsigned long long)(KRAFT_HTTP_MAX_BODY / KRAFT_SIZE_MB(1)));
        } else if (body_error) {
            response->ok = false;
            response->error = StringFormat(arena, "%s (%llu B read)", body_message, (unsigned long long)total);
        } else {
            *(u8*)arena->Push(1, 1, false) = 0;
            response->body = String8{{body_start}, total};
            response->body_size = total;
            response->ok = true;
        }
    }

done: {
    HINTERNET owned_request = (HINTERNET)AtomicExchangePointer(&task->request_handle, nullptr);
    if (owned_request) {
        if (!response->timings.measured) {
            HttpReadTimings(owned_request, 0, 0, &response->timings);
        }

        WinHttpCloseHandle(owned_request);
    }
}
    HttpCloseSwap(&task->connection);
    if (AtomicLoad32(&task->timed_out) != 0) {
        response->ok = false;
        response->error = StringFormat(arena, "timed out after %u ms", task->request.timeout_ms);
    } else if (AtomicLoad32(&task->cancelled) != 0) {
        response->ok = false;
        response->error = S("cancelled");
    }

    response->elapsed_micros = (u64)((Platform::GetAbsoluteTime() - start_time) * 1000000.0);
    response->elapsed_ms = response->elapsed_micros / 1000;
    response->timings.total_micros = response->elapsed_micros;
    if (response->ok && task->request.after_response) {
        task->request.after_response(response, task->request.after_response_user);
    }

    AtomicStore32(&task->done, 1);
}

HttpTask* HttpSend(HttpRequest request) {
    HttpSessionGet();
    ArenaAllocator* arena = CreateArena({.ChunkSize = KRAFT_HTTP_MAX_BODY + KRAFT_SIZE_MB(64), .Alignment = KRAFT_SIZE_KB(64)});
    HttpTask* task = ArenaPush(arena, HttpTask);
    task->arena = arena;
    task->done = 0;
    task->request.method = request.method;
    task->request.timeout_ms = request.timeout_ms;
    task->deadline = request.timeout_ms > 0 ? Platform::GetAbsoluteTime() + request.timeout_ms / 1000.0 : 0.0;
    task->request.no_redirects = request.no_redirects;
    task->request.after_response = request.after_response;
    task->request.after_response_user = request.after_response_user;
    task->request.before_redirect = request.before_redirect;
    task->request.before_redirect_user = request.before_redirect_user;
    task->request.max_redirects = request.max_redirects;
    task->request.url = StringCopy(arena, request.url);
    task->request.body = request.body.count ? StringCopy(arena, request.body) : String8{};
    if (request.headers && request.header_count > 0) {
        task->request.headers = ArenaPushArray(arena, HttpHeader, request.header_count);
        task->request.header_count = request.header_count;
        for (u32 index = 0; index < request.header_count; index++) {
            task->request.headers[index].key = StringCopy(arena, request.headers[index].key);
            task->request.headers[index].value = StringCopy(arena, request.headers[index].value);
        }
    }

    task->thread = ThreadLaunch(HttpThreadProc, task, S("http request"));
    if (!task->thread.handle) {
        task->response.ok = false;
        task->response.error = S("failed to start request thread");
        AtomicStore32(&task->done, 1);
    }

    return task;
}

bool HttpTaskDone(HttpTask* task) {
    if (AtomicLoad32(&task->done) != 0) {
        return true;
    }

    // Timeout?
    if (task->deadline > 0.0 && Platform::GetAbsoluteTime() >= task->deadline && AtomicCompareExchange32(&task->timed_out, 1, 0) == 0) {
        HttpCloseSwap(&task->request_handle);
        HttpCloseSwap(&task->connection);
    }

    return false;
}

HttpResponse HttpTaskResult(HttpTask* task) {
    return task->response;
}

// Closing the handles makes the blocked WinHTTP call fail at once
void HttpTaskCancel(HttpTask* task) {
    if (AtomicLoad32(&task->done) != 0) {
        return;
    }

    AtomicStore32(&task->cancelled, 1);
    HttpCloseSwap(&task->request_handle);
    HttpCloseSwap(&task->connection);
}

void HttpTaskDestroy(HttpTask* task) {
    if (task->thread.handle) {
        if (AtomicLoad32(&task->done) == 0) {
            HttpCloseSwap(&task->request_handle);
            HttpCloseSwap(&task->connection);
        }

        ThreadJoin(task->thread);
    }

    DestroyArena(task->arena);
}

} // namespace kraft::net
