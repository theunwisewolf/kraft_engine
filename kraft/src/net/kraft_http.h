#pragma once

#include <core/kraft_core.h>

namespace kraft {
struct ArenaAllocator;

namespace net {

enum HttpMethod {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD,
};

struct HttpHeader {
    String8 key;
    String8 value;
};

// In microseconds
// Any phase that was skipped remains 0
struct NetTimings {
    b32 measured; // false when the system can't report the phases, only the total
    b32 reused; // the request went over a connection that was already open
    b32 opened_ahead; // that connection was opened before the request, to be ready for it
    u64 dns_micros;
    u64 connect_micros;
    u64 tls_micros;
    u64 send_micros;
    u64 wait_micros;
    u64 receive_micros;
    u64 total_micros;
};

struct HttpResponse;

// A redirect about to be followed
struct HttpRedirect {
    i32 status;
    String8 url;

    // the redirect's own response headers
    HttpHeader* headers;
    u32 header_count;

    // What before_redirect allocates next_headers from
    ArenaAllocator* arena;

    // Set by before_redirect: headers for the request to url, each replacing any header of the same name.
    // An empty value removes that header instead
    HttpHeader* next_headers;
    u32 next_header_count;
};

struct HttpRequest {
    HttpMethod method;
    String8 url;
    String8 body;
    HttpHeader* headers;
    u32 header_count;

    // 0 means system default
    u32 timeout_ms;
    b32 no_redirects;

    // 0 means system defualt
    u32 max_redirects;

    // Runs on the request's thread before the task is done
    void (*after_response)(HttpResponse* response, void* user);
    void* after_response_user;

    // Runs on a thread of kraft's before each redirect is followed
    void (*before_redirect)(HttpRedirect* redirect, void* user);
    void* before_redirect_user;
};

#define KRAFT_HTTP_MAX_BODY KRAFT_SIZE_MB(1024)

struct HttpResponse {
    b32 ok;
    i32 status;
    String8 status_text;
    String8 body;
    String8 error;
    u64 elapsed_ms;
    u64 elapsed_micros;
    u64 body_size;
    HttpHeader* headers;
    u32 header_count;
    String8 raw_headers;

    // set when the request only went through with certificate verification off
    String8 tls_warning;
    NetTimings timings;
};

struct HttpTask;

// Returns the method as text, like "GET"
KRAFT_API const char* HttpMethodName(HttpMethod method);

// Default user-agent is "kraft" if this is not called before the first request
KRAFT_API void HttpSetUserAgent(String8 user_agent);

// Sends the request on a thread of its own and returns at once
// Poll HttpTaskDone, read the response with HttpTaskResult, then free the task with HttpTaskDestroy
// No cookies are kept: a request carries only the Cookie header it is given
KRAFT_API HttpTask* HttpSend(HttpRequest request);

// Returns true once the request has finished, failed, timed out or been cancelled
KRAFT_API bool HttpTaskDone(HttpTask* task);

// Returns the response of a finished task, which stays valid until HttpTaskDestroy
KRAFT_API HttpResponse HttpTaskResult(HttpTask* task);

// Stops the request; the task finishes shortly after with "cancelled" as its error
KRAFT_API void HttpTaskCancel(HttpTask* task);

// Waits for the task's thread to finish and frees everything the task allocated
KRAFT_API void HttpTaskDestroy(HttpTask* task);

// Closes the connections kept open for later requests; call it once when the app exits
KRAFT_API void HttpShutdown();

} // namespace net

} // namespace kraft
