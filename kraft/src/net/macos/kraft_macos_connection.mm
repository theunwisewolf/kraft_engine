#include <net/kraft_connection.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_threading.h>
#include <platform/macos/kraft_macos_cocoa.h>

#include <arpa/inet.h>
#include <dns_sd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <core/kraft_core_includes.h>

namespace kraft::net {

struct ConnectionBuffer {
    ArenaAllocator* arena;
    u8* data;
    u64 count;
    u64 capacity;
};

struct ConnectionPlatform {
    // -1 when there is none
    int socket;

    // the address ConnectionOpen reached
    sockaddr_storage peer;
    ConnectionBuffer plaintext;

    // Network.framework cannot take over a connected socket, so once ConnectionStartTls has run the connection is
    // one of its own, and every wait on it goes through a semaphore its handlers signal
    nw_connection_t tls;
    dispatch_queue_t queue;
    dispatch_semaphore_t state_changed;
    dispatch_semaphore_t sent;
    dispatch_semaphore_t received;
    volatile i32 state;
    nw_error_t state_error;
    nw_error_t send_error;
    bool receive_pending;

    // the pending receive has finished, and what it brought is waiting below
    bool receive_ready;
    dispatch_data_t receive_content;
    bool receive_complete;
    nw_error_t receive_error;
    bool peer_closed;
    bool verify_failed;
    char verify_reason[256];

    // for the handlers, which cannot take arrays from the stack
    char host[512];
    char alpn[64];
};

static void ConnectionBufferReserve(ConnectionBuffer* buffer, u64 extra) {
    if (buffer->count + extra <= buffer->capacity) {
        return;
    }

    u64 new_capacity = buffer->capacity ? buffer->capacity * 2 : 16384;
    while (new_capacity < buffer->count + extra) {
        new_capacity *= 2;
    }

    u8* new_data = ArenaPushArrayNoZero(buffer->arena, u8, new_capacity);
    if (buffer->count) {
        MemCpy(new_data, buffer->data, buffer->count);
    }

    buffer->data = new_data;
    buffer->capacity = new_capacity;
}

static String8 MacSocketErrorText(ArenaAllocator* arena, int code) {
    const char* known = nullptr;
    switch (code) {
    case ECONNREFUSED:  known = "connection refused: nothing is listening on that port"; break;
    case ETIMEDOUT:     known = "timed out"; break;
    case ECONNRESET:    known = "the connection was reset by the peer"; break;
    case ECONNABORTED:  known = "the connection was aborted"; break;
    case EPIPE:         known = "the connection was closed by the peer"; break;
    case EHOSTUNREACH:  known = "no route to that host"; break;
    case ENETUNREACH:   known = "the network is unreachable"; break;
    case ENETDOWN:      known = "the network is down"; break;
    case ENETRESET:     known = "the connection was reset when the network dropped"; break;
    case EADDRNOTAVAIL: known = "that address is not available on this machine"; break;
    case EADDRINUSE:    known = "that address is already in use"; break;
    case ENOTCONN:      known = "the socket is not connected"; break;
    case ESHUTDOWN:     known = "the socket has already been shut down"; break;
    case EACCES:        known = "permission denied"; break;
    case EINTR:         known = "interrupted"; break;
    case EINVAL:        known = "invalid argument"; break;
    case EMFILE:        known = "no socket handles left"; break;
    case ENOBUFS:       known = "no buffer space left"; break;
    case EWOULDBLOCK:   known = "the operation would block"; break;
    case EMSGSIZE:      known = "the message was too large for the buffer"; break;
    case ENOTSOCK:      known = "the handle is not a socket"; break;
    case ECANCELED:     known = "cancelled"; break;
    default:            break;
    }

    if (known) {
        return StringFormat(arena, "%s (errno %d)", known, code);
    }

    char text[128];
    if (strerror_r(code, text, sizeof(text)) != 0) {
        return StringFormat(arena, "errno %d", code);
    }

    return StringFormat(arena, "%s (errno %d)", text, code);
}

// Why a host name could not be looked up, from Network.framework's DNS error
static const char* MacDnsErrorText(int code) {
    switch (code) {
    case kDNSServiceErr_NoSuchName:
    case kDNSServiceErr_NoSuchRecord:       return "no such host";
    case kDNSServiceErr_Timeout:            return "the lookup timed out";
    case kDNSServiceErr_Refused:            return "the name server refused the lookup";
    case kDNSServiceErr_ServiceNotRunning:  return "the system's resolver is not running";
    default:                                return "the lookup failed";
    }
}

// Why getaddrinfo could not look a host name up
static const char* MacResolveErrorText(int code) {
    switch (code) {
    case EAI_NONAME:    return "no such host";
    case EAI_AGAIN:     return "the name server is temporarily unavailable, try again";
    case EAI_FAIL:      return "the name server reported a non-recoverable error";
    default:            return gai_strerror(code);
    }
}

static String8 MacNetworkErrorText(ArenaAllocator* arena, nw_error_t error) {
    if (!error) {
        return S("the connection failed");
    }

    int code = nw_error_get_error_code(error);
    switch (nw_error_get_error_domain(error)) {
    case nw_error_domain_posix: return MacSocketErrorText(arena, code);
    case nw_error_domain_dns:   return StringFormat(arena, "the host name could not be resolved: %s (dns %d)", MacDnsErrorText(code), code);
    case nw_error_domain_tls:   {
        const char* known = "TLS handshake failed";
        switch (code) {
        case errSSLProtocol:
        case errSSLHandshakeFail:
        case errSSLRecordOverflow:      known = "the TLS handshake was rejected by the server (is this a TLS port?)"; break;
        case errSSLPeerHandshakeFail:
        case errSSLNegotiation:         known = "no TLS version or cipher in common with the server"; break;
        case errSSLPeerProtocolVersion: known = "no TLS version in common with the server, or it is not a TLS port"; break;
        case errSSLClosedAbort:         known = "the server closed the connection during the TLS handshake"; break;
        case errSSLPeerNoRenegotiation: known = "server requested TLS renegotiation, which is not supported"; break;
        default:                        break;
        }

        return StringFormat(arena, "%s (OSStatus %d)", known, code);
    }
    default: break;
    }

    return StringFormat(arena, "the connection failed (error %d)", code);
}

static void MacFailTimedOut(Connection* connection) {
    ConnectionFail(
        connection,
        "timed out after %u s while %s (%llu bytes received)",
        connection->timeout_ms / 1000,
        connection->phase ? connection->phase : "waiting",
        (unsigned long long)connection->bytes_received
    );
}

// Waits until the socket can do `events`, in short steps so a cancel is noticed, giving up at the deadline
static bool MacSocketWaitFor(Connection* connection, short events) {
    int socket = connection->platform->socket;
    for (;;) {
        if (ConnectionCancelled(connection)) {
            ConnectionFail(connection, "cancelled");
            return false;
        }

        f64 now = Platform::GetAbsoluteTime();
        if (now >= connection->deadline) {
            MacFailTimedOut(connection);
            return false;
        }

        f64 remaining = connection->deadline - now;
        int wait_ms = remaining > 0.2 ? 200 : (int)(remaining * 1000.0) + 1;
        struct pollfd poll_entry = {socket, events, 0};
        int ready = poll(&poll_entry, 1, wait_ms);
        if (ready < 0 && errno != EINTR) {
            ConnectionFail(connection, "waiting on the socket failed: %S", MacSocketErrorText(connection->arena, errno));
            return false;
        }

        // An error or a hang-up shows in the send or receive that follows
        if (ready > 0) {
            return true;
        }
    }
}

static bool MacSocketSendAll(Connection* connection, const u8* data, u64 count) {
    while (count > 0) {
        if (!MacSocketWaitFor(connection, POLLOUT)) {
            return false;
        }

        ssize_t sent = send(connection->platform->socket, data, (size_t)(count > (1 << 20) ? (1 << 20) : count), 0);
        if (sent < 0) {
            if (errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }

            ConnectionFail(connection, "send failed: %S", MacSocketErrorText(connection->arena, errno));
            return false;
        }

        data += sent;
        count -= (u64)sent;
    }

    return true;
}

static i64 MacSocketReceiveSome(Connection* connection, ConnectionBuffer* buffer) {
    for (;;) {
        if (!MacSocketWaitFor(connection, POLLIN)) {
            return -1;
        }

        ConnectionBufferReserve(buffer, 65536);
        ssize_t received = recv(connection->platform->socket, buffer->data + buffer->count, (size_t)(buffer->capacity - buffer->count), 0);
        if (received < 0) {
            if (errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }

            ConnectionFail(connection, "receive failed: %S", MacSocketErrorText(connection->arena, errno));
            return -1;
        }

        buffer->count += (u64)received;
        connection->bytes_received += (u64)received;

        return received;
    }
}

static void MacConnectionPrepare(Connection* connection) {
    if (!connection->platform) {
        connection->platform = ArenaPush(connection->arena, ConnectionPlatform);
        connection->platform->socket = -1;
        connection->platform->plaintext.arena = connection->arena;
    }
}

// A socket that never raises SIGPIPE, which would end the program when the peer has gone, and never blocks
static void MacSocketSetUp(int socket) {
    int on = 1;
    setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
    fcntl(socket, F_SETFL, fcntl(socket, F_GETFL, 0) | O_NONBLOCK);
    fcntl(socket, F_SETFD, FD_CLOEXEC);
}

// Copies `host` into `out_text` with a zero after it, without the brackets of an IPv6 address like [::1]
static void MacHostText(String8 host, char* out_text, u64 capacity) {
    u64 host_length = host.count < capacity - 1 ? host.count : capacity - 1;
    MemCpy(out_text, host.ptr, host_length);
    out_text[host_length] = 0;
    if (host_length > 2 && out_text[0] == '[' && out_text[host_length - 1] == ']') {
        memmove(out_text, out_text + 1, host_length - 2);
        out_text[host_length - 2] = 0;
    }
}

#define KRAFT_MAC_CONNECT_MAX_SOCKETS 64

bool ConnectionOpen(Connection* connection, String8 host, u16 port, u64* out_dns_micros) {
    MacConnectionPrepare(connection);

    char host_text[512];
    MacHostText(host, host_text, sizeof(host_text));
    char port_text[8];
    snprintf(port_text, sizeof(port_text), "%u", (u32)port);

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* results = nullptr;
    f64 resolve_start = Platform::GetAbsoluteTime();
    int resolve_status = getaddrinfo(host_text, port_text, &hints, &results);
    *out_dns_micros = (u64)((Platform::GetAbsoluteTime() - resolve_start) * 1000000.0);
    if (resolve_status != 0 || !results) {
        ConnectionFail(connection, "could not resolve %s: %s (gai %d)", host_text, MacResolveErrorText(resolve_status), resolve_status);
        return false;
    }

    // Start with IPv4
    // localhost often has both, and a proxy (Docker, a VM) may accept on one family without forwarding anywhere
    int pending[KRAFT_MAC_CONNECT_MAX_SOCKETS];
    addrinfo* pending_address[KRAFT_MAC_CONNECT_MAX_SOCKETS];
    u32 pending_count = 0;
    bool has_ipv4 = false;
    for (addrinfo* candidate = results; candidate; candidate = candidate->ai_next) {
        has_ipv4 = has_ipv4 || candidate->ai_family == AF_INET;
    }

    bool ipv6_started = !has_ipv4;
    f64 ipv6_start = Platform::GetAbsoluteTime() + 0.2;
    int last_connect_error = 0;
    auto start_family = [&](int family) {
        for (addrinfo* candidate = results; candidate && pending_count < KRAFT_MAC_CONNECT_MAX_SOCKETS; candidate = candidate->ai_next) {
            if (candidate->ai_family != family) {
                continue;
            }

            int candidate_socket = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
            if (candidate_socket < 0) {
                continue;
            }

            MacSocketSetUp(candidate_socket);
            if (connect(candidate_socket, candidate->ai_addr, candidate->ai_addrlen) != 0 && errno != EINPROGRESS) {
                last_connect_error = errno;
                close(candidate_socket);
                continue;
            }

            pending[pending_count] = candidate_socket;
            pending_address[pending_count] = candidate;
            pending_count++;
        }
    };

    start_family(AF_INET);
    if (ipv6_started) {
        start_family(AF_INET6);
    }

    int connected_socket = -1;
    while ((pending_count > 0 || !ipv6_started) && connected_socket < 0) {
        if (ConnectionCancelled(connection)) {
            ConnectionFail(connection, "cancelled");
            break;
        }

        f64 now = Platform::GetAbsoluteTime();
        if (!ipv6_started && (pending_count == 0 || now >= ipv6_start)) {
            ipv6_started = true;
            start_family(AF_INET6);
            if (pending_count == 0) {
                break;
            }
        }

        if (now >= connection->deadline) {
            ConnectionFail(connection, "timed out after %u s connecting to %s:%u", connection->timeout_ms / 1000, host_text, port);
            break;
        }

        f64 remaining_seconds = connection->deadline - now;
        if (!ipv6_started && ipv6_start - now < remaining_seconds) {
            remaining_seconds = ipv6_start - now;
        }

        int wait_ms = remaining_seconds > 0.2 ? 200 : (int)(remaining_seconds * 1000.0) + 1;
        struct pollfd poll_entries[KRAFT_MAC_CONNECT_MAX_SOCKETS];
        for (u32 index = 0; index < pending_count; index++) {
            poll_entries[index] = {pending[index], POLLOUT, 0};
        }

        int ready = poll(poll_entries, pending_count, wait_ms);
        if (ready < 0 && errno != EINTR) {
            ConnectionFail(connection, "waiting on the socket failed: %S", MacSocketErrorText(connection->arena, errno));
            break;
        }

        for (u32 index = 0; index < pending_count && ready > 0;) {
            if (poll_entries[index].revents == 0) {
                index++;
                continue;
            }

            int candidate_socket = pending[index];
            int socket_error = 0;
            socklen_t error_length = sizeof(socket_error);
            getsockopt(candidate_socket, SOL_SOCKET, SO_ERROR, &socket_error, &error_length);
            if (socket_error == 0 && (poll_entries[index].revents & POLLOUT) && connected_socket < 0) {
                connected_socket = candidate_socket;
                MemCpy(&connection->platform->peer, pending_address[index]->ai_addr, pending_address[index]->ai_addrlen);
            } else {
                if (socket_error != 0) {
                    last_connect_error = socket_error;
                }

                close(candidate_socket);
            }

            pending_count--;
            pending[index] = pending[pending_count];
            pending_address[index] = pending_address[pending_count];
            poll_entries[index] = poll_entries[pending_count];
        }
    }

    for (u32 index = 0; index < pending_count; index++) {
        close(pending[index]);
    }

    freeaddrinfo(results);

    if (connected_socket < 0) {
        if (connection->error.count == 0 && last_connect_error != 0) {
            ConnectionFail(connection, "could not connect to %s:%u: %S", host_text, port, MacSocketErrorText(connection->arena, last_connect_error));
        } else if (connection->error.count == 0) {
            ConnectionFail(connection, "could not connect to %s:%u", host_text, port);
        }
        return false;
    }

    int no_delay = 1;
    setsockopt(connected_socket, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
    connection->platform->socket = connected_socket;

    return true;
}

//
// TLS, through Network.framework
//

// Waits for one signal of the semaphore, in short steps so a cancel is noticed, giving up at the deadline
static bool MacTlsWait(Connection* connection, dispatch_semaphore_t semaphore) {
    for (;;) {
        if (ConnectionCancelled(connection)) {
            ConnectionFail(connection, "cancelled");
            return false;
        }

        f64 now = Platform::GetAbsoluteTime();
        if (now >= connection->deadline) {
            MacFailTimedOut(connection);
            return false;
        }

        f64 remaining = connection->deadline - now;
        if (remaining > 0.2) {
            remaining = 0.2;
        }

        if (dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, (i64)(remaining * NSEC_PER_SEC) + 1)) == 0) {
            return true;
        }
    }
}

static void MacTlsRelease(ConnectionPlatform* platform) {
    if (platform->state_error) {
        nw_release(platform->state_error);
    }

    if (platform->send_error) {
        nw_release(platform->send_error);
    }

    if (platform->receive_error) {
        nw_release(platform->receive_error);
    }

    if (platform->receive_content) {
        dispatch_release(platform->receive_content);
    }

    nw_release(platform->tls);
    dispatch_release(platform->state_changed);
    dispatch_release(platform->sent);
    dispatch_release(platform->received);
    dispatch_release(platform->queue);
    platform->tls = nullptr;
    platform->state_error = nullptr;
    platform->send_error = nullptr;
    platform->receive_error = nullptr;
    platform->receive_content = nullptr;
    platform->receive_pending = false;
    platform->receive_ready = false;
}

// Cancelling ends with the cancelled state, after every other handler has run and before none can run again
static void MacTlsClose(ConnectionPlatform* platform) {
    nw_connection_cancel(platform->tls);
    while (AtomicLoad32(&platform->state) != nw_connection_state_cancelled) {
        dispatch_semaphore_wait(platform->state_changed, DISPATCH_TIME_FOREVER);
    }

    dispatch_sync(platform->queue, ^{});
    MacTlsRelease(platform);
}

// Where it went wrong, named the way ConnectionOpen names it, since a connection made in one step also looks up the
// host and connects to it
static void MacTlsFail(Connection* connection, u16 port) {
    ConnectionPlatform* platform = connection->platform;
    nw_error_t error = platform->state_error;
    nw_error_domain_t domain = error ? nw_error_get_error_domain(error) : nw_error_domain_invalid;
    if (domain == nw_error_domain_dns) {
        int code = nw_error_get_error_code(error);
        ConnectionFail(connection, "could not resolve %s: %s (dns %d)", platform->host, MacDnsErrorText(code), code);
    } else if (domain == nw_error_domain_posix) {
        ConnectionFail(connection, "could not connect to %s:%u: %S", platform->host, (u32)port, MacNetworkErrorText(connection->arena, error));
    } else {
        ConnectionFail(connection, "%S", MacNetworkErrorText(connection->arena, error));
    }
}

// Makes the TLS connection to `endpoint` and waits until it is ready with `alpn` agreed
static bool MacTlsConnect(Connection* connection, nw_endpoint_t endpoint, u16 port, String8 host, String8 alpn, bool skip_verify) {
    ConnectionPlatform* platform = connection->platform;
    MacHostText(host, platform->host, sizeof(platform->host));
    u64 alpn_count = alpn.count < sizeof(platform->alpn) - 1 ? alpn.count : sizeof(platform->alpn) - 1;
    MemCpy(platform->alpn, alpn.ptr, alpn_count);
    platform->alpn[alpn_count] = 0;

    // A certificate is only named by its address when the host is one, so there is no name to send for it
    u8 address_bytes[16];
    bool host_is_address = inet_pton(AF_INET, platform->host, address_bytes) == 1 || inet_pton(AF_INET6, platform->host, address_bytes) == 1;

    platform->queue = dispatch_queue_create("kraft.tls", DISPATCH_QUEUE_SERIAL);
    platform->state_changed = dispatch_semaphore_create(0);
    platform->sent = dispatch_semaphore_create(0);
    platform->received = dispatch_semaphore_create(0);
    platform->state = nw_connection_state_invalid;
    platform->verify_failed = false;
    platform->peer_closed = false;

    nw_parameters_t parameters = nw_parameters_create_secure_tcp(
        ^(nw_protocol_options_t tls_options) {
          sec_protocol_options_t options = nw_tls_copy_sec_protocol_options(tls_options);
          sec_protocol_options_set_min_tls_protocol_version(options, tls_protocol_version_TLSv12);
          if (!host_is_address) {
              sec_protocol_options_set_tls_server_name(options, platform->host);
          }

          if (alpn_count > 0) {
              sec_protocol_options_add_tls_application_protocol(options, platform->alpn);
          }

          // The certificate is judged here rather than by the handshake, so a self-signed development server
          // still connects, with a warning
          sec_protocol_options_set_verify_block(
              options,
              ^(sec_protocol_metadata_t metadata, sec_trust_t trust, sec_protocol_verify_complete_t complete) {
                if (!skip_verify) {
                    SecTrustRef trust_ref = sec_trust_copy_ref(trust);
                    CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault, platform->host, kCFStringEncodingUTF8);
                    SecPolicyRef policy = SecPolicyCreateSSL(true, name);
                    SecTrustSetPolicies(trust_ref, policy);
                    CFErrorRef error = nullptr;
                    if (!SecTrustEvaluateWithError(trust_ref, &error)) {
                        platform->verify_failed = true;
                        MacTrustFailureText(error, platform->verify_reason, sizeof(platform->verify_reason));
                    }

                    if (error) {
                        CFRelease(error);
                    }

                    CFRelease(policy);
                    CFRelease(name);
                    CFRelease(trust_ref);
                }

                complete(true);
              },
              platform->queue
          );
          sec_release(options);
        },
        ^(nw_protocol_options_t tcp_options) { nw_tcp_options_set_no_delay(tcp_options, true); }
    );

    platform->tls = nw_connection_create(endpoint, parameters);
    nw_release(parameters);

    nw_connection_set_queue(platform->tls, platform->queue);
    nw_connection_set_state_changed_handler(platform->tls, ^(nw_connection_state_t state, nw_error_t error) {
      if (error && !platform->state_error) {
          nw_retain(error);
          platform->state_error = error;
      }

      AtomicStore32(&platform->state, (i32)state);
      dispatch_semaphore_signal(platform->state_changed);
    });
    nw_connection_start(platform->tls);

    // Waiting means the connection could not be made now and would be tried again later, which is a failure here
    bool ready = false;
    while (!ready) {
        if (!MacTlsWait(connection, platform->state_changed)) {
            break;
        }

        i32 state = AtomicLoad32(&platform->state);
        if (state == nw_connection_state_ready) {
            ready = true;
        } else if (state == nw_connection_state_failed || state == nw_connection_state_waiting || state == nw_connection_state_cancelled) {
            MacTlsFail(connection, port);
            break;
        }
    }

    if (ready && alpn_count > 0) {
        nw_protocol_definition_t definition = nw_protocol_copy_tls_definition();
        nw_protocol_metadata_t metadata = nw_connection_copy_protocol_metadata(platform->tls, definition);
        sec_protocol_metadata_t tls_metadata = metadata ? nw_tls_copy_sec_protocol_metadata(metadata) : nullptr;
        const char* negotiated = tls_metadata ? sec_protocol_metadata_get_negotiated_protocol(tls_metadata) : nullptr;
        if (!negotiated || strcmp(negotiated, platform->alpn) != 0) {
            ConnectionFail(connection, "the server did not agree to %S over TLS (ALPN)", alpn);
            ready = false;
        }

        if (tls_metadata) {
            sec_release(tls_metadata);
        }

        if (metadata) {
            nw_release(metadata);
        }

        nw_release(definition);
    }

    if (!ready) {
        MacTlsClose(platform);
        return false;
    }

    if (platform->verify_failed) {
        connection->tls_warning = StringFormat(connection->arena, "TLS not verified: %s", platform->verify_reason);
    }

    return true;
}

bool ConnectionStartTls(Connection* connection, String8 host, String8 alpn, bool skip_verify) {
    ConnectionPlatform* platform = connection->platform;
    if (!platform || platform->socket < 0) {
        ConnectionFail(connection, "TLS needs an open connection");
        return false;
    }

    // Network.framework makes its own connection to the address ConnectionOpen reached, so the plain one goes
    close(platform->socket);
    platform->socket = -1;

    nw_endpoint_t endpoint = nw_endpoint_create_address((const struct sockaddr*)&platform->peer);
    u16 port = ntohs(((const sockaddr_in*)&platform->peer)->sin_port);
    bool secured = MacTlsConnect(connection, endpoint, port, host, alpn, skip_verify);
    nw_release(endpoint);

    return secured;
}

// What the system's report on making the connection says each part took
static void MacTlsTimes(ConnectionPlatform* platform, ConnectionTimes* out_times) {
    dispatch_semaphore_t reported = dispatch_semaphore_create(0);
    __block ConnectionTimes times = {};
    nw_connection_access_establishment_report(platform->tls, platform->queue, ^(nw_establishment_report_t report) {
      if (report) {
          nw_establishment_report_enumerate_resolutions(
              report,
              ^bool(nw_report_resolution_source_t source, uint64_t milliseconds, uint32_t endpoint_count, nw_endpoint_t successful, nw_endpoint_t preferred) {
                times.dns_micros += milliseconds * 1000;
                return true;
              }
          );
          nw_protocol_definition_t tcp = nw_protocol_copy_tcp_definition();
          nw_protocol_definition_t tls = nw_protocol_copy_tls_definition();
          nw_establishment_report_enumerate_protocols(report, ^bool(nw_protocol_definition_t protocol, uint64_t handshake_milliseconds, uint64_t handshake_rtt_milliseconds) {
            if (nw_protocol_definition_is_equal(protocol, tcp)) {
                times.connect_micros = handshake_milliseconds * 1000;
            } else if (nw_protocol_definition_is_equal(protocol, tls)) {
                times.tls_micros = handshake_milliseconds * 1000;
            }
            return true;
          });
          nw_release(tcp);
          nw_release(tls);
      }
      dispatch_semaphore_signal(reported);
    });
    if (dispatch_semaphore_wait(reported, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC)) == 0) {
        *out_times = times;
    }
    dispatch_release(reported);
}

// One connection by the host's name, so the TCP and TLS handshakes are not two connections' worth of round trips.
// localhost goes in two steps: ConnectionOpen tries IPv4 first, which a proxy listening on both families needs.
bool ConnectionOpenTls(Connection* connection, String8 host, u16 port, String8 alpn, bool skip_verify, ConnectionTimes* out_times) {
    MacConnectionPrepare(connection);
    char host_text[512];
    MacHostText(host, host_text, sizeof(host_text));
    if (strcasecmp(host_text, "localhost") == 0) {
        return ConnectionOpenTlsInSteps(connection, host, port, alpn, skip_verify, out_times);
    }

    *out_times = {};
    char port_text[8];
    snprintf(port_text, sizeof(port_text), "%u", (u32)port);
    f64 start = Platform::GetAbsoluteTime();
    const char* phase = connection->phase;
    connection->phase = "connecting and completing the TLS handshake";
    nw_endpoint_t endpoint = nw_endpoint_create_host(host_text, port_text);
    bool secured = MacTlsConnect(connection, endpoint, port, host, alpn, skip_verify);
    nw_release(endpoint);
    connection->phase = phase;
    if (!secured) {
        out_times->connect_micros = (u64)((Platform::GetAbsoluteTime() - start) * 1000000.0);
        return false;
    }

    MacTlsTimes(connection->platform, out_times);
    return true;
}

static void MacTlsStartReceive(ConnectionPlatform* platform) {
    if (platform->receive_pending) {
        return;
    }

    platform->receive_pending = true;
    nw_connection_receive(platform->tls, 1, KRAFT_SIZE_KB(256), ^(dispatch_data_t content, nw_content_context_t context, bool is_complete, nw_error_t error) {
      if (content) {
          dispatch_retain(content);
          platform->receive_content = content;
      }

      if (error) {
          nw_retain(error);
          platform->receive_error = error;
      }

      platform->receive_complete = is_complete;
      dispatch_semaphore_signal(platform->received);
    });
}

// What the finished receive brought, copied out into the plaintext buffer
static i64 MacTlsTakeReceive(Connection* connection, const u8** out_data) {
    ConnectionPlatform* platform = connection->platform;
    platform->receive_pending = false;
    platform->receive_ready = false;
    platform->plaintext.count = 0;
    if (platform->receive_content) {
        dispatch_data_apply(platform->receive_content, ^bool(dispatch_data_t region, size_t offset, const void* bytes, size_t size) {
          ConnectionBufferReserve(&platform->plaintext, size);
          MemCpy(platform->plaintext.data + platform->plaintext.count, bytes, size);
          platform->plaintext.count += size;
          return true;
        });
        dispatch_release(platform->receive_content);
        platform->receive_content = nullptr;
    }

    if (platform->receive_complete) {
        platform->peer_closed = true;
    }

    *out_data = platform->plaintext.data;
    connection->bytes_received += platform->plaintext.count;
    if (platform->plaintext.count > 0) {
        return (i64)platform->plaintext.count;
    }

    if (platform->receive_error) {
        int code = nw_error_get_error_code(platform->receive_error);
        // The peer closing without a TLS goodbye is still the end of the data
        if (nw_error_get_error_domain(platform->receive_error) == nw_error_domain_posix && (code == ECONNRESET || code == ENODATA)) {
            platform->peer_closed = true;
            return 0;
        }

        ConnectionFail(connection, "receive failed: %S", MacNetworkErrorText(connection->arena, platform->receive_error));
        return -1;
    }

    return 0;
}

bool ConnectionSend(Connection* connection, const u8* data, u64 count) {
    ConnectionPlatform* platform = connection->platform;
    if (!platform->tls) {
        return MacSocketSendAll(connection, data, count);
    }

    if (count == 0) {
        return true;
    }

    dispatch_data_t content = dispatch_data_create(data, (size_t)count, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    nw_connection_send(platform->tls, content, NW_CONNECTION_DEFAULT_STREAM_CONTEXT, false, ^(nw_error_t error) {
      if (error && !platform->send_error) {
          nw_retain(error);
          platform->send_error = error;
      }

      dispatch_semaphore_signal(platform->sent);
    });
    dispatch_release(content);

    if (!MacTlsWait(connection, platform->sent)) {
        return false;
    }

    if (platform->send_error) {
        ConnectionFail(connection, "send failed: %S", MacNetworkErrorText(connection->arena, platform->send_error));
        return false;
    }

    return true;
}

i64 ConnectionReceive(Connection* connection, const u8** out_data) {
    ConnectionPlatform* platform = connection->platform;
    if (!platform->tls) {
        platform->plaintext.count = 0;
        i64 received = MacSocketReceiveSome(connection, &platform->plaintext);
        *out_data = platform->plaintext.data;
        return received;
    }

    platform->plaintext.count = 0;
    *out_data = platform->plaintext.data;
    if (platform->peer_closed && !platform->receive_ready) {
        return 0;
    }

    for (;;) {
        if (!platform->receive_ready) {
            MacTlsStartReceive(platform);
            if (!MacTlsWait(connection, platform->received)) {
                return -1;
            }

            platform->receive_ready = true;
        }

        i64 received = MacTlsTakeReceive(connection, out_data);
        if (received != 0 || platform->peer_closed || connection->error.count > 0) {
            return received;
        }
    }
}

i32 ConnectionReadableWithin(Connection* connection, u64 micros) {
    ConnectionPlatform* platform = connection->platform;
    if (platform->tls) {
        if (platform->receive_ready || platform->peer_closed) {
            return 1;
        }

        MacTlsStartReceive(platform);
        if (dispatch_semaphore_wait(platform->received, dispatch_time(DISPATCH_TIME_NOW, (i64)micros * NSEC_PER_USEC)) != 0) {
            return 0;
        }

        platform->receive_ready = true;
        return 1;
    }

    struct pollfd poll_entry = {platform->socket, POLLIN, 0};
    int ready = poll(&poll_entry, 1, (int)((micros + 999) / 1000));
    if (ready < 0 && errno != EINTR) {
        ConnectionFail(connection, "waiting on the socket failed: %S", MacSocketErrorText(connection->arena, errno));
        return -1;
    }

    return ready > 0 ? 1 : 0;
}

bool ConnectionIsOpen(Connection* connection) {
    return connection->platform && (connection->platform->tls || connection->platform->socket >= 0);
}

void ConnectionClose(Connection* connection) {
    ConnectionPlatform* platform = connection->platform;
    if (!platform) {
        return;
    }

    if (platform->tls) {
        MacTlsClose(platform);
    }

    if (platform->socket >= 0) {
        close(platform->socket);
    }

    platform->socket = -1;
}

//
// Listener
//

#define KRAFT_LISTENER_MAX_SOCKETS 8

struct Listener {
    ArenaAllocator* arena;
    int sockets[KRAFT_LISTENER_MAX_SOCKETS];
    u32 socket_count;
    u16 port;
};

// Whether something accepts connections at the address, as another program's listener on that port would
static bool MacSomethingListens(sockaddr_storage* address, socklen_t length) {
    int probe = ::socket(address->ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (probe < 0) {
        return false;
    }

    MacSocketSetUp(probe);
    bool listening = connect(probe, (sockaddr*)address, length) == 0;
    if (!listening && errno == EINPROGRESS) {
        struct pollfd poll_entry = {probe, POLLOUT, 0};
        int socket_error = 0;
        socklen_t error_length = sizeof(socket_error);
        listening = poll(&poll_entry, 1, 200) > 0 && getsockopt(probe, SOL_SOCKET, SO_ERROR, &socket_error, &error_length) == 0 && socket_error == 0;
    }

    close(probe);

    return listening;
}

static int MacListenOn(addrinfo* address, u16 port, int* out_error) {
    sockaddr_storage bound_address = {};
    MemCpy(&bound_address, address->ai_addr, address->ai_addrlen);
    if (address->ai_family == AF_INET) {
        ((sockaddr_in*)&bound_address)->sin_port = htons(port);
    } else {
        ((sockaddr_in6*)&bound_address)->sin6_port = htons(port);
    }

    // Without SO_REUSEADDR first: with it, a BSD system binds an address over another program listening on every
    // address. It is only for a port that a closed connection still lingers on.
    for (u32 attempt = 0; attempt < 2; attempt++) {
        int listening = ::socket(address->ai_family, SOCK_STREAM, IPPROTO_TCP);
        if (listening < 0) {
            *out_error = errno;
            return -1;
        }

        int on = 1;
        if (attempt == 1) {
            setsockopt(listening, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        }

        if (address->ai_family == AF_INET6) {
            setsockopt(listening, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
        }

        if (bind(listening, (sockaddr*)&bound_address, address->ai_addrlen) == 0 && listen(listening, SOMAXCONN) == 0) {
            MacSocketSetUp(listening);
            return listening;
        }

        *out_error = errno;
        close(listening);
        if (*out_error != EADDRINUSE || port == 0 || MacSomethingListens(&bound_address, address->ai_addrlen)) {
            return -1;
        }
    }

    return -1;
}

Listener* ListenerStart(String8 host, u16 port, ListenerStartStatus* out_status) {
    ListenerStartStatus ignored_status;
    if (!out_status) {
        out_status = &ignored_status;
    }

    *out_status = LISTENER_START_STATUS_FAILED;

    char host_text[512];
    MacHostText(host, host_text, sizeof(host_text));
    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    addrinfo* results = nullptr;
    if (getaddrinfo(host.count > 0 ? host_text : nullptr, "0", &hints, &results) != 0 || !results) {
        return nullptr;
    }

    ArenaAllocator* arena = ArenaAlloc();
    Listener* listener = ArenaPush(arena, Listener);
    listener->arena = arena;
    listener->port = port;

    // IPv4 first, so a free port is picked there and IPv6 takes the same one
    int families[] = {AF_INET, AF_INET6};
    int last_error = 0;
    for (u32 family_index = 0; family_index < KRAFT_C_ARRAY_SIZE(families); family_index++) {
        for (addrinfo* address = results; address && listener->socket_count < KRAFT_LISTENER_MAX_SOCKETS; address = address->ai_next) {
            if (address->ai_family != families[family_index]) {
                continue;
            }

            int listening = MacListenOn(address, listener->port, &last_error);
            if (listening < 0) {
                continue;
            }

            if (listener->port == 0) {
                sockaddr_storage bound_address = {};
                socklen_t size = sizeof(bound_address);
                getsockname(listening, (sockaddr*)&bound_address, &size);
                listener->port = ntohs(bound_address.ss_family == AF_INET ? ((sockaddr_in*)&bound_address)->sin_port : ((sockaddr_in6*)&bound_address)->sin6_port);
            }

            listener->sockets[listener->socket_count++] = listening;
        }
    }

    freeaddrinfo(results);

    if (listener->socket_count == 0) {
        *out_status = last_error == EADDRINUSE || last_error == EACCES ? LISTENER_START_STATUS_PORT_TAKEN : LISTENER_START_STATUS_FAILED;
        DestroyArena(arena);
        return nullptr;
    }

    *out_status = LISTENER_START_STATUS_OK;

    return listener;
}

u16 ListenerPort(Listener* listener) {
    return listener->port;
}

i32 ListenerAccept(Listener* listener, u64 timeout_ms, Connection* out_connection) {
    struct pollfd poll_entries[KRAFT_LISTENER_MAX_SOCKETS];
    for (u32 index = 0; index < listener->socket_count; index++) {
        poll_entries[index] = {listener->sockets[index], POLLIN, 0};
    }

    int ready = poll(poll_entries, listener->socket_count, (int)timeout_ms);
    if (ready < 0) {
        return errno == EINTR ? 0 : -1;
    }

    for (u32 index = 0; index < listener->socket_count && ready > 0; index++) {
        if (!(poll_entries[index].revents & POLLIN)) {
            continue;
        }

        // The peer may have given up between the two calls
        int accepted = accept(listener->sockets[index], nullptr, nullptr);
        if (accepted < 0) {
            continue;
        }

        MacSocketSetUp(accepted);
        MacConnectionPrepare(out_connection);
        out_connection->platform->socket = accepted;

        return 1;
    }

    return 0;
}

void ListenerStop(Listener* listener) {
    if (!listener) {
        return;
    }

    for (u32 index = 0; index < listener->socket_count; index++) {
        close(listener->sockets[index]);
    }

    DestroyArena(listener->arena);
}

} // namespace kraft::net
