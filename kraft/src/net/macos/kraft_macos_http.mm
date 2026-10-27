#include <net/kraft_http.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_threading.h>
#include <platform/macos/kraft_macos_cocoa.h>

#include <os/lock.h>

#include <core/kraft_core_includes.h>

namespace kraft::net {
struct HttpTask;
}

// What one request's transfer reports, written on the session's queue and read by the request's thread once it is over
@interface KraftHttpDelegate : NSObject <NSURLSessionDataDelegate> {
@public
    // cleared under the lock once the request's thread is done with the delegate
    kraft::net::HttpTask* task;
    os_unfair_lock lock;
    dispatch_semaphore_t finished;
    dispatch_semaphore_t measured;
    NSHTTPURLResponse* response;
    NSURLSessionTaskMetrics* metrics;
    NSError* error;
    u8* body_start;
    u64 body_count;
    u32 redirect_count;
    bool too_large;
    bool too_many_redirects;

    // a certificate this request's own connections met did not verify
    bool tls_unverified;
    char tls_reason[256];
    f64 headers_time;
}
@end

namespace kraft::net {

struct HttpTask {
    Thread thread;
    volatile i32 done;
    ArenaAllocator* arena;
    HttpRequest request;
    HttpResponse response;
    volatile i32 cancelled;
    volatile i32 timed_out;
    f64 deadline;
    NSURLSessionDataTask* session_task;
    KraftHttpDelegate* delegate;
};

// NSURLSession keeps connections open per session, so one session for the app's lifetime
static NSURLSession* g_http_session;
static NSOperationQueue* g_http_queue;
static os_unfair_lock g_http_session_lock = OS_UNFAIR_LOCK_INIT;
static char g_http_user_agent[128] = "kraft";

static NSURLSession* HttpSessionGet() {
    os_unfair_lock_lock(&g_http_session_lock);
    if (!g_http_session) {
        // Like WinHTTP: no cookies kept, nothing cached
        NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        configuration.URLCache = nil;
        configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
        configuration.HTTPCookieStorage = nil;
        configuration.HTTPShouldSetCookies = NO;
        configuration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
        g_http_queue = [[NSOperationQueue alloc] init];
        g_http_queue.maxConcurrentOperationCount = 1;
        g_http_queue.name = @"kraft http";
        g_http_session = [[NSURLSession sessionWithConfiguration:configuration delegate:nil delegateQueue:g_http_queue] retain];
    }
    NSURLSession* session = g_http_session;
    os_unfair_lock_unlock(&g_http_session_lock);

    return session;
}

void HttpSetUserAgent(String8 user_agent) {
    u64 count = user_agent.count < sizeof(g_http_user_agent) - 1 ? user_agent.count : sizeof(g_http_user_agent) - 1;
    MemCpy(g_http_user_agent, user_agent.ptr, count);
    g_http_user_agent[count] = 0;
}

void HttpShutdown() {
    os_unfair_lock_lock(&g_http_session_lock);
    if (g_http_session) {
        [g_http_session invalidateAndCancel];
        [g_http_session release];
        [g_http_queue release];
        g_http_session = nil;
        g_http_queue = nil;
    }
    os_unfair_lock_unlock(&g_http_session_lock);
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

// NSHTTPURLResponse keeps no reason phrase, and HTTP/2 sends none, so the standard one for the code
static const char* HttpReasonPhrase(i32 status) {
    switch (status) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 207: return "Multi-Status";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Content Too Large";
    case 414: return "URI Too Long";
    case 415: return "Unsupported Media Type";
    case 416: return "Range Not Satisfiable";
    case 417: return "Expectation Failed";
    case 418: return "I'm a teapot";
    case 421: return "Misdirected Request";
    case 422: return "Unprocessable Content";
    case 423: return "Locked";
    case 424: return "Failed Dependency";
    case 425: return "Too Early";
    case 426: return "Upgrade Required";
    case 428: return "Precondition Required";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 451: return "Unavailable For Legal Reasons";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 505: return "HTTP Version Not Supported";
    case 507: return "Insufficient Storage";
    case 508: return "Loop Detected";
    case 511: return "Network Authentication Required";
    }

    return "";
}

static String8 HttpErrorText(ArenaAllocator* arena, NSError* error) {
    const char* known = nullptr;
    if ([error.domain isEqualToString:NSURLErrorDomain]) {
        switch (error.code) {
        case NSURLErrorTimedOut:                                     known = "timed out"; break;
        case NSURLErrorBadURL:                                       known = "the URL is not valid"; break;
        case NSURLErrorUnsupportedURL:                               known = "the URL scheme is not http or https"; break;
        case NSURLErrorCannotFindHost:                               known = "the host name could not be resolved"; break;
        case NSURLErrorDNSLookupFailed:                              known = "the host name could not be resolved"; break;
        case NSURLErrorCannotConnectToHost:                          known = "could not connect: nothing is listening, or a firewall refused it"; break;
        case NSURLErrorNetworkConnectionLost:                        known = "the connection was reset"; break;
        case NSURLErrorNotConnectedToInternet:                       known = "this machine is not connected to a network"; break;
        case NSURLErrorCancelled:                                    known = "cancelled"; break;
        case NSURLErrorUserCancelledAuthentication:                  known = "authentication was rejected"; break;
        case NSURLErrorBadServerResponse:                            known = "the reply was not valid HTTP"; break;
        case NSURLErrorCannotParseResponse:                          known = "the reply was not valid HTTP"; break;
        case NSURLErrorCannotDecodeRawData:                          known = "the response could not be decompressed"; break;
        case NSURLErrorCannotDecodeContentData:                      known = "the response could not be decompressed"; break;
        case NSURLErrorHTTPTooManyRedirects:                         known = "too many redirects, or one that could not be followed"; break;
        case NSURLErrorRedirectToNonExistentLocation:                known = "too many redirects, or one that could not be followed"; break;
        case NSURLErrorSecureConnectionFailed:                       known = "the TLS handshake failed"; break;
        case NSURLErrorServerCertificateHasBadDate:                  known = "the server's certificate has expired or is not valid yet"; break;
        case NSURLErrorServerCertificateNotYetValid:                 known = "the server's certificate has expired or is not valid yet"; break;
        case NSURLErrorServerCertificateUntrusted:                   known = "the certificate was signed by an authority this machine does not trust"; break;
        case NSURLErrorServerCertificateHasUnknownRoot:              known = "the certificate was signed by an authority this machine does not trust"; break;
        case NSURLErrorClientCertificateRejected:                    known = "the server asked for a client certificate"; break;
        case NSURLErrorClientCertificateRequired:                    known = "the server asked for a client certificate"; break;
        case NSURLErrorAppTransportSecurityRequiresSecureConnection: known = "App Transport Security refused a plain http connection"; break;
        case NSURLErrorDataLengthExceedsMaximum:                     known = "NSURLSession refused to send the request with its body"; break;
        default:                                                     break;
        }
    }

    if (known) {
        return StringFormat(arena, "%s (NSURLError %ld)", known, (long)error.code);
    }

    const char* description = error.localizedDescription.UTF8String;

    return StringFormat(arena, "%s (%s %ld)", description ? description : "unknown error", error.domain.UTF8String, (long)error.code);
}

//
// Certificates that did not verify
//

// A connection is checked once and then kept for the requests after it, which never see the check, so what it
// found is remembered for its host and port
#define KRAFT_HTTP_UNTRUSTED_MAX 32

struct HttpUntrustedHost {
    char host[256];
    i32 port;
    char reason[256];
};

static HttpUntrustedHost g_http_untrusted[KRAFT_HTTP_UNTRUSTED_MAX];
static u32 g_http_untrusted_count;
static os_unfair_lock g_http_untrusted_lock = OS_UNFAIR_LOCK_INIT;

static i32 HttpUntrustedFind(const char* host, i32 port) {
    for (u32 index = 0; index < g_http_untrusted_count; index++) {
        if (g_http_untrusted[index].port == port && strcmp(g_http_untrusted[index].host, host) == 0) {
            return (i32)index;
        }
    }

    return -1;
}

static void HttpUntrustedRemember(const char* host, i32 port, const char* reason) {
    os_unfair_lock_lock(&g_http_untrusted_lock);
    i32 found = HttpUntrustedFind(host, port);
    if (!reason && found >= 0) {
        g_http_untrusted[found] = g_http_untrusted[--g_http_untrusted_count];
    } else if (reason) {
        if (found < 0) {
            found = (i32)(g_http_untrusted_count < KRAFT_HTTP_UNTRUSTED_MAX ? g_http_untrusted_count++ : KRAFT_HTTP_UNTRUSTED_MAX - 1);
        }

        snprintf(g_http_untrusted[found].host, sizeof(g_http_untrusted[found].host), "%s", host);
        g_http_untrusted[found].port = port;
        snprintf(g_http_untrusted[found].reason, sizeof(g_http_untrusted[found].reason), "%s", reason);
    }
    os_unfair_lock_unlock(&g_http_untrusted_lock);
}

static bool HttpUntrustedReason(const char* host, i32 port, char* out_reason, u64 capacity) {
    os_unfair_lock_lock(&g_http_untrusted_lock);
    i32 found = HttpUntrustedFind(host, port);
    if (found >= 0) {
        snprintf(out_reason, capacity, "%s", g_http_untrusted[found].reason);
    }
    os_unfair_lock_unlock(&g_http_untrusted_lock);

    return found >= 0;
}

static i32 HttpDefaultPort(NSURL* url) {
    if (url.port) {
        return url.port.intValue;
    }

    return [url.scheme.lowercaseString isEqualToString:@"https"] ? 443 : 80;
}

static u64 HttpMicrosBetween(NSDate* from, NSDate* to) {
    if (!from || !to) {
        return 0;
    }

    NSTimeInterval seconds = [to timeIntervalSinceDate:from];

    return seconds > 0.0 ? (u64)(seconds * 1000000.0) : 0;
}

static void HttpReadTimings(NSURLSessionTaskMetrics* metrics, NetTimings* out) {
    NSURLSessionTaskTransactionMetrics* transaction = metrics.transactionMetrics.lastObject;
    if (!transaction) {
        return;
    }

    out->measured = true;
    out->reused = transaction.isReusedConnection;
    out->dns_micros = HttpMicrosBetween(transaction.domainLookupStartDate, transaction.domainLookupEndDate);
    out->connect_micros = HttpMicrosBetween(transaction.connectStartDate, transaction.secureConnectionStartDate ? transaction.secureConnectionStartDate : transaction.connectEndDate);
    out->tls_micros = HttpMicrosBetween(transaction.secureConnectionStartDate, transaction.secureConnectionEndDate);
    out->send_micros = HttpMicrosBetween(transaction.requestStartDate, transaction.requestEndDate);
    out->wait_micros = HttpMicrosBetween(transaction.requestEndDate, transaction.responseStartDate);
    out->receive_micros = HttpMicrosBetween(transaction.responseStartDate, transaction.responseEndDate);
}

static const char* HttpVersionText(NSURLSessionTaskMetrics* metrics) {
    NSString* protocol = metrics.transactionMetrics.lastObject.networkProtocolName;
    if ([protocol isEqualToString:@"h2"]) {
        return "HTTP/2";
    }

    if ([protocol isEqualToString:@"h3"]) {
        return "HTTP/3";
    }

    if ([protocol isEqualToString:@"http/1.0"]) {
        return "HTTP/1.0";
    }

    return "HTTP/1.1";
}

static void HttpAppendWithoutLineBreaks(NSMutableString* out, String8 text) {
    NSString* whole = [[[NSString alloc] initWithBytes:text.ptr length:text.count encoding:NSUTF8StringEncoding] autorelease];
    if (!whole) {
        whole = [[[NSString alloc] initWithBytes:text.ptr length:text.count encoding:NSISOLatin1StringEncoding] autorelease];
    }

    for (NSString* part in [whole componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"\r\n"]]) {
        [out appendString:part];
    }
}

static NSURL* HttpUrl(String8 url) {
    NSString* text = [[[NSString alloc] initWithBytes:url.ptr length:url.count encoding:NSUTF8StringEncoding] autorelease];
    if (!text) {
        return nil;
    }

    NSURL* parsed = [NSURL URLWithString:text];
    if (!parsed) {
        // Spaces and other characters a URL may not hold as they are, which WinHTTP takes
        NSMutableCharacterSet* allowed = [[[NSCharacterSet URLQueryAllowedCharacterSet] mutableCopy] autorelease];
        [allowed addCharactersInString:@"#%"];
        parsed = [NSURL URLWithString:[text stringByAddingPercentEncodingWithAllowedCharacters:allowed]];
    }

    return parsed;
}

static void HttpFail(HttpTask* task, const char* reason, String8 detail) {
    task->response.ok = false;
    task->response.error = detail.count > 0 ? StringFormat(task->arena, "%s: %S", reason, detail) : StringFormat(task->arena, "%s", reason);
}

// NSHTTPURLResponse joins every Set-Cookie into one value with commas, which an Expires date has in it too. A comma
// starts the next cookie when a name and its = follow it before any ; or ,
static void HttpAppendField(NSMutableString* raw, NSString* key, NSString* value) {
    if ([key caseInsensitiveCompare:@"Set-Cookie"] != NSOrderedSame) {
        [raw appendFormat:@"%@: %@\r\n", key, value];
        return;
    }

    NSUInteger length = value.length;
    NSUInteger start = 0;
    for (NSUInteger index = 0; index <= length; index++) {
        b32 split = index == length;
        if (!split && [value characterAtIndex:index] == ',') {
            NSUInteger name_start = index + 1;
            while (name_start < length && [value characterAtIndex:name_start] == ' ') {
                name_start++;
            }

            NSUInteger name_end = name_start;
            while (name_end < length) {
                unichar character = [value characterAtIndex:name_end];
                if (character == '=' || character == ';' || character == ',' || character == ' ') {
                    break;
                }
                name_end++;
            }
            split = name_end > name_start && name_end < length && [value characterAtIndex:name_end] == '=';
        }

        if (!split) {
            continue;
        }

        NSString* cookie = [[value substringWithRange:NSMakeRange(start, index - start)] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        if (cookie.length > 0) {
            [raw appendFormat:@"%@: %@\r\n", key, cookie];
        }
        start = index + 1;
    }
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

static void HttpCancelTask(HttpTask* task) {
    NSURLSessionDataTask* session_task = (NSURLSessionDataTask*)AtomicExchangePointer((void* volatile*)&task->session_task, nullptr);
    if (session_task) {
        [session_task cancel];
        [session_task release];
    }
}

static NSMutableURLRequest* HttpBuildRequest(HttpTask* task, NSURL* url) {
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
    request.HTTPMethod = [NSString stringWithUTF8String:HttpMethodName(task->request.method)];
    if (task->request.timeout_ms > 0) {
        request.timeoutInterval = task->request.timeout_ms / 1000.0;
    }

    b32 has_content_type = false;
    b32 has_user_agent = false;
    for (u32 index = 0; index < task->request.header_count; index++) {
        HttpHeader header = task->request.headers[index];
        NSMutableString* key = [NSMutableString string];
        NSMutableString* value = [NSMutableString string];
        HttpAppendWithoutLineBreaks(key, header.key);
        HttpAppendWithoutLineBreaks(value, header.value);
        if (key.length == 0) {
            continue;
        }

        has_content_type = has_content_type || [key caseInsensitiveCompare:@"Content-Type"] == NSOrderedSame;
        has_user_agent = has_user_agent || [key caseInsensitiveCompare:@"User-Agent"] == NSOrderedSame;
        [request addValue:value forHTTPHeaderField:key];
    }

    if (task->request.body.count > 0 && !has_content_type) {
        [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    }

    if (!has_user_agent) {
        [request setValue:[NSString stringWithUTF8String:g_http_user_agent] forHTTPHeaderField:@"User-Agent"];
    }

    if (task->request.body.count > 0) {
        request.HTTPBody = [NSData dataWithBytes:task->request.body.ptr length:task->request.body.count];
    }

    request.HTTPShouldHandleCookies = NO;

    return request;
}

// Called on the session's queue under the delegate's lock, while the request's thread waits for the task to end
static NSURLRequest* HttpRedirected(HttpTask* task, NSHTTPURLResponse* redirect_response, NSURLRequest* next) {
    ArenaAllocator* arena = task->arena;
    HttpRedirect redirect = {};
    redirect.arena = arena;
    redirect.status = (i32)redirect_response.statusCode;
    const char* next_url = next.URL.absoluteString.UTF8String;
    redirect.url = next_url ? StringCopy(arena, String8FromCString(next_url)) : String8{};

    NSMutableString* raw = [NSMutableString stringWithFormat:@"HTTP/1.1 %d\r\n", redirect.status];
    NSDictionary* fields = redirect_response.allHeaderFields;
    for (NSString* key in [fields.allKeys sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]) {
        HttpAppendField(raw, key, fields[key]);
    }
    HttpParseHeaders(arena, StringCopy(arena, String8FromCString(raw.UTF8String)), &redirect.headers, &redirect.header_count);
    task->request.before_redirect(&redirect, task->request.before_redirect_user);
    if (redirect.next_header_count == 0) {
        return next;
    }

    NSMutableURLRequest* changed = [[next mutableCopy] autorelease];
    for (u32 index = 0; index < redirect.next_header_count; index++) {
        NSMutableString* key = [NSMutableString string];
        NSMutableString* value = [NSMutableString string];
        HttpAppendWithoutLineBreaks(key, redirect.next_headers[index].key);
        HttpAppendWithoutLineBreaks(value, redirect.next_headers[index].value);
        if (key.length > 0) {
            [changed setValue:(value.length > 0 ? value : nil) forHTTPHeaderField:key];
        }
    }
    changed.HTTPShouldHandleCookies = NO;

    return changed;
}

// Everything the transfer left in the delegate, turned into the response, on the request's own thread
static void HttpTakeResult(HttpTask* task, KraftHttpDelegate* delegate) {
    ArenaAllocator* arena = task->arena;
    HttpResponse* response = &task->response;

    // The body went into the arena as it arrived, so it is the last thing in it and the zero goes right after
    u8* terminator = (u8*)arena->Push(1, 1, false);
    *terminator = 0;
    response->body = String8{{delegate->body_start ? delegate->body_start : terminator}, delegate->body_count};
    response->body_size = delegate->body_count;

    HttpReadTimings(delegate->metrics, &response->timings);
    NSHTTPURLResponse* http = delegate->response;
    if (http) {
        response->status = (i32)http.statusCode;
        response->status_text = StringCopy(arena, String8FromCString(HttpReasonPhrase(response->status)));

        NSMutableString* raw = [NSMutableString stringWithFormat:@"%s %d %s\r\n", HttpVersionText(delegate->metrics), response->status, HttpReasonPhrase(response->status)];
        NSDictionary* fields = http.allHeaderFields;
        for (NSString* key in [fields.allKeys sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]) {
            HttpAppendField(raw, key, fields[key]);
        }
        [raw appendString:@"\r\n"];
        response->raw_headers = StringCopy(arena, String8FromCString(raw.UTF8String));
        HttpParseHeaders(arena, response->raw_headers, &response->headers, &response->header_count);
    }

    // Every host the request went through counts, a redirect's too, and a connection kept open from an earlier
    // request was checked then, so the hosts whose certificates did not verify are looked up as well
    char reason[256] = {};
    os_unfair_lock_lock(&delegate->lock);
    bool unverified = delegate->tls_unverified;
    if (unverified) {
        snprintf(reason, sizeof(reason), "%s", delegate->tls_reason);
    }
    os_unfair_lock_unlock(&delegate->lock);
    for (NSURLSessionTaskTransactionMetrics* transaction in delegate->metrics.transactionMetrics) {
        NSURL* url = transaction.request.URL;
        if (!unverified && url.host && [url.scheme.lowercaseString isEqualToString:@"https"]) {
            unverified = HttpUntrustedReason(url.host.UTF8String, HttpDefaultPort(url), reason, sizeof(reason));
        }
    }

    if (!unverified && http.URL.host && [http.URL.scheme.lowercaseString isEqualToString:@"https"]) {
        unverified = HttpUntrustedReason(http.URL.host.UTF8String, HttpDefaultPort(http.URL), reason, sizeof(reason));
    }

    if (unverified) {
        response->tls_warning = StringFormat(arena, "TLS not verified: %s", reason);
    }

    if (delegate->too_large) {
        response->ok = false;
        response->error = StringFormat(arena, "the response is larger than %llu MB, the most that is read", (unsigned long long)(KRAFT_HTTP_MAX_BODY / KRAFT_SIZE_MB(1)));
    } else if (delegate->too_many_redirects) {
        HttpFail(task, "receive failed", S("too many redirects, or one that could not be followed"));
    } else if (delegate->error) {
        HttpFail(task, http ? "receive failed" : "send failed", HttpErrorText(arena, delegate->error));
    } else if (!http) {
        HttpFail(task, "receive failed", S("the reply was not valid HTTP"));
    } else {
        response->ok = true;
    }
}

static KRAFT_THREAD_ENTRY(HttpThreadProc) {
    HttpTask* task = (HttpTask*)user_data;
    HttpResponse* response = &task->response;
    *response = {};
    f64 start_time = Platform::GetAbsoluteTime();

    @autoreleasepool {
        NSURL* url = HttpUrl(task->request.url);
        NSString* scheme = url.scheme.lowercaseString;
        if (!url || !url.host) {
            HttpFail(task, "invalid URL", S("the URL is not valid"));
        } else if (![scheme isEqualToString:@"http"] && ![scheme isEqualToString:@"https"]) {
            HttpFail(task, "invalid URL", S("the URL scheme is not http or https"));
        } else if (task->request.method == HTTP_METHOD_GET && task->request.body.count > 0) {
            // NSURLSession refuses a GET with a body, where WinHTTP sends one; leaving the body out would send another request
            HttpFail(task, "send failed", S("a GET with a body cannot be sent on a Mac, where NSURLSession refuses one; send it with no body, or as POST"));
        } else {
            KraftHttpDelegate* delegate = [[KraftHttpDelegate alloc] init];
            delegate->task = task;
            delegate->lock = OS_UNFAIR_LOCK_INIT;
            delegate->finished = dispatch_semaphore_create(0);
            delegate->measured = dispatch_semaphore_create(0);
            task->delegate = delegate;

            NSURLSessionDataTask* session_task = [[HttpSessionGet() dataTaskWithRequest:HttpBuildRequest(task, url)] retain];
            session_task.delegate = delegate;
            [session_task retain];
            AtomicExchangePointer((void* volatile*)&task->session_task, session_task);
            if (AtomicLoad32(&task->cancelled) != 0 || AtomicLoad32(&task->timed_out) != 0) {
                HttpCancelTask(task);
            }
            [session_task resume];

            // The deadline is kept here too, so it holds without anyone polling the task
            bool finished = false;
            while (!finished) {
                f64 remaining = task->deadline > 0.0 ? task->deadline - Platform::GetAbsoluteTime() : 1.0;
                if (remaining <= 0.0) {
                    if (AtomicCompareExchange32(&task->timed_out, 1, 0) == 0) {
                        HttpCancelTask(task);
                    }
                    remaining = 1.0;
                }

                dispatch_time_t wait = dispatch_time(DISPATCH_TIME_NOW, (i64)((remaining < 1.0 ? remaining : 1.0) * NSEC_PER_SEC) + 1);
                finished = dispatch_semaphore_wait(delegate->finished, wait) == 0;
            }

            // The metrics arrive before the end of a task; a short wait in case a system ever sends them after
            dispatch_semaphore_wait(delegate->measured, dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC));

            os_unfair_lock_lock(&delegate->lock);
            delegate->task = nullptr;
            os_unfair_lock_unlock(&delegate->lock);
            HttpTakeResult(task, delegate);
            [session_task release];
        }
    }

    if (AtomicLoad32(&task->timed_out) != 0) {
        response->ok = false;
        response->error = StringFormat(task->arena, "timed out after %u ms", task->request.timeout_ms);
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

    if (task->deadline > 0.0 && Platform::GetAbsoluteTime() >= task->deadline && AtomicCompareExchange32(&task->timed_out, 1, 0) == 0) {
        HttpCancelTask(task);
    }

    return false;
}

HttpResponse HttpTaskResult(HttpTask* task) {
    return task->response;
}

void HttpTaskCancel(HttpTask* task) {
    if (AtomicLoad32(&task->done) != 0) {
        return;
    }

    AtomicStore32(&task->cancelled, 1);
    HttpCancelTask(task);
}

void HttpTaskDestroy(HttpTask* task) {
    if (task->thread.handle) {
        if (AtomicLoad32(&task->done) == 0) {
            HttpCancelTask(task);
        }

        ThreadJoin(task->thread);
    }

    HttpCancelTask(task);
    if (task->delegate) {
        [task->delegate release];
        task->delegate = nil;
    }

    DestroyArena(task->arena);
}

} // namespace kraft::net

@implementation KraftHttpDelegate

- (void)dealloc {
    [response release];
    [metrics release];
    [error release];
    if (finished) {
        dispatch_release(finished);
    }

    if (measured) {
        dispatch_release(measured);
    }

    [super dealloc];
}

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)session_task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)redirect
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completion {
    NSURLRequest* follow = request;
    os_unfair_lock_lock(&lock);
    if (task) {
        u32 limit = task->request.max_redirects > 0 ? task->request.max_redirects : 10;
        if (task->request.no_redirects) {
            follow = nil;
        } else if (++redirect_count > limit) {
            too_many_redirects = true;
            follow = nil;
        } else if (task->request.before_redirect) {
            follow = kraft::net::HttpRedirected(task, redirect, request);
        }
    }
    os_unfair_lock_unlock(&lock);

    completion(follow);
}

// A certificate that does not verify lets the request through, and the response says so in tls_warning. The check
// can wait on the network, for a missing intermediate or an answer about revocation, so it runs off the session's
// queue, which every request's callbacks go through.
- (void)URLSession:(NSURLSession*)session
                   task:(NSURLSessionTask*)session_task
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))completion {
    NSURLProtectionSpace* space = challenge.protectionSpace;
    if (![space.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust] || !space.serverTrust) {
        completion(NSURLSessionAuthChallengePerformDefaultHandling, nil);
        return;
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
      @autoreleasepool {
          SecTrustRef trust = challenge.protectionSpace.serverTrust;
          CFErrorRef trust_error = nullptr;
          bool trusted = SecTrustEvaluateWithError(trust, &trust_error);
          char reason[256];
          if (!trusted) {
              kraft::MacTrustFailureText(trust_error, reason, sizeof(reason));
          }

          if (trust_error) {
              CFRelease(trust_error);
          }

          kraft::net::HttpUntrustedRemember(challenge.protectionSpace.host.UTF8String, (i32)challenge.protectionSpace.port, trusted ? nullptr : reason);
          if (trusted) {
              completion(NSURLSessionAuthChallengePerformDefaultHandling, nil);
              return;
          }

          os_unfair_lock_lock(&lock);
          tls_unverified = true;
          snprintf(tls_reason, sizeof(tls_reason), "%s", reason);
          os_unfair_lock_unlock(&lock);
          completion(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:trust]);
      }
    });
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)data_task
    didReceiveResponse:(NSURLResponse*)received
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completion {
    os_unfair_lock_lock(&lock);
    if (task && [received isKindOfClass:[NSHTTPURLResponse class]]) {
        [response release];
        response = (NSHTTPURLResponse*)[received retain];
        headers_time = kraft::Platform::GetAbsoluteTime();
    }
    os_unfair_lock_unlock(&lock);

    completion(NSURLSessionResponseAllow);
}

// The body goes straight into the task's arena, all of it in one run
- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)data_task didReceiveData:(NSData*)data {
    os_unfair_lock_lock(&lock);
    kraft::net::HttpTask* receiver = task;
    if (receiver && !too_large) {
        [data enumerateByteRangesUsingBlock:^(const void* bytes, NSRange range, BOOL* stop) {
          kraft::ArenaAllocator* arena = receiver->arena;
          if (body_count + range.length > KRAFT_HTTP_MAX_BODY || arena->position + range.length + 1 > arena->capacity) {
              too_large = true;
              *stop = YES;
              return;
          }

          u8* destination = (u8*)arena->Push(range.length, 1, false);
          if (!body_start) {
              body_start = destination;
          }

          kraft::MemCpy(destination, bytes, range.length);
          body_count += range.length;
        }];
    }

    bool cancel = too_large;
    os_unfair_lock_unlock(&lock);
    if (cancel) {
        [data_task cancel];
    }
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)session_task didFinishCollectingMetrics:(NSURLSessionTaskMetrics*)collected {
    os_unfair_lock_lock(&lock);
    [metrics release];
    metrics = [collected retain];
    os_unfair_lock_unlock(&lock);

    dispatch_semaphore_signal(measured);
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)session_task didCompleteWithError:(NSError*)failure {
    os_unfair_lock_lock(&lock);
    [error release];
    error = [failure retain];
    os_unfair_lock_unlock(&lock);

    dispatch_semaphore_signal(finished);
}

@end
