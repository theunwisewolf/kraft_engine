#include <net/kraft_connection.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_threading.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define SECURITY_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <security.h>
#include <schannel.h>

#include <stdarg.h>

#include <core/kraft_core_includes.h>

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "secur32")
#pragma comment(lib, "crypt32")

namespace kraft::net {

struct ConnectionBuffer {
    ArenaAllocator* arena;
    u8* data;
    u64 count;
    u64 capacity;
};

struct ConnectionPlatform {
    SOCKET socket;
    bool tls;
    CredHandle credentials;
    CtxtHandle context;
    bool has_credentials;
    bool has_context;
    SecPkgContext_StreamSizes sizes;
    ConnectionBuffer encrypted;
    ConnectionBuffer plaintext;
    u8* send_scratch;
};

static void Win32WinsockStartup() {
    static bool started = false;
    if (!started) {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
        started = true;
    }
}

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

static void ConnectionBufferAppend(ConnectionBuffer* buffer, const u8* bytes, u64 count) {
    ConnectionBufferReserve(buffer, count);
    if (count) {
        MemCpy(buffer->data + buffer->count, bytes, count);
    }

    buffer->count += count;
}

static void ConnectionBufferConsume(ConnectionBuffer* buffer, u64 count) {
    if (count >= buffer->count) {
        buffer->count = 0;
        return;
    }

    memmove(buffer->data, buffer->data + count, buffer->count - count);
    buffer->count -= count;
}

static String8 Win32SocketErrorText(ArenaAllocator* arena, int code) {
    const char* known = nullptr;
    switch (code) {
    case WSAECONNREFUSED:   known = "connection refused: nothing is listening on that port"; break;
    case WSAETIMEDOUT:      known = "timed out"; break;
    case WSAECONNRESET:     known = "the connection was reset by the peer"; break;
    case WSAECONNABORTED:   known = "the connection was aborted"; break;
    case WSAEHOSTUNREACH:   known = "no route to that host"; break;
    case WSAENETUNREACH:    known = "the network is unreachable"; break;
    case WSAENETDOWN:       known = "the network is down"; break;
    case WSAENETRESET:      known = "the connection was reset when the network dropped"; break;
    case WSAEADDRNOTAVAIL:  known = "that address is not available on this machine"; break;
    case WSAEADDRINUSE:     known = "that address is already in use"; break;
    case WSAENOTCONN:       known = "the socket is not connected"; break;
    case WSAESHUTDOWN:      known = "the socket has already been shut down"; break;
    case WSAEACCES:         known = "permission denied"; break;
    case WSAEINTR:          known = "interrupted"; break;
    case WSAEINVAL:         known = "invalid argument"; break;
    case WSAEMFILE:         known = "no socket handles left"; break;
    case WSAENOBUFS:        known = "no buffer space left"; break;
    case WSAEWOULDBLOCK:    known = "the operation would block"; break;
    case WSAEMSGSIZE:       known = "the message was too large for the buffer"; break;
    case WSAHOST_NOT_FOUND: known = "the host name could not be resolved"; break;
    case WSANO_DATA:        known = "the host name has no address record"; break;
    case WSATRY_AGAIN:      known = "the name server is temporarily unavailable, try again"; break;
    case WSANO_RECOVERY:    known = "the name server reported a non-recoverable error"; break;
    case WSAENOTSOCK:       known = "the handle is not a socket"; break;
    case WSASYSNOTREADY:    known = "the network subsystem is not ready"; break;
    default:                break;
    }

    if (known) {
        return StringFormat(arena, "%s (wsa %d)", known, code);
    }

    wchar_t* wide = nullptr;
    DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, (DWORD)code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&wide, 0, nullptr
    );
    if (length == 0 || !wide) {
        return StringFormat(arena, "wsa error %d", code);
    }

    while (length > 0 && (wide[length - 1] == L'\n' || wide[length - 1] == L'\r' || wide[length - 1] == L'.' || wide[length - 1] == L' ')) {
        wide[--length] = 0;
    }

    i32 bytes = WideCharToMultiByte(CP_UTF8, 0, wide, (i32)length, nullptr, 0, nullptr, nullptr);
    String8 message = {};
    if (bytes > 0) {
        message.ptr = ArenaPushArrayNoZero(arena, u8, (u64)bytes + 1);
        WideCharToMultiByte(CP_UTF8, 0, wide, (i32)length, (char*)message.ptr, bytes, nullptr, nullptr);
        message.count = (u64)bytes;
        message.ptr[bytes] = 0;
    }
    LocalFree(wide);

    if (message.count == 0) {
        return StringFormat(arena, "wsa error %d", code);
    }

    return StringFormat(arena, "%S (wsa %d)", message, code);
}

void ConnectionFail(Connection* connection, const char* format, ...) {
    if (connection->error.count) {
        return;
    }

    va_list args;
    va_start(args, format);
    connection->error = StringFormatV(connection->arena, format, args);
    va_end(args);
}

bool ConnectionCancelled(Connection* connection) {
    return connection->cancel && AtomicLoad32(connection->cancel) != 0;
}

enum Win32SocketWait {
    WIN32_SOCKET_WAIT_READ,
    WIN32_SOCKET_WAIT_WRITE,
    WIN32_SOCKET_WAIT_CONNECT,
};

static bool Win32SocketWaitFor(Connection* connection, Win32SocketWait kind, bool* out_connect_failed = nullptr) {
    SOCKET socket = connection->platform->socket;
    for (;;) {
        if (ConnectionCancelled(connection)) {
            ConnectionFail(connection, "cancelled");
            return false;
        }

        f64 now = Platform::GetAbsoluteTime();
        if (now >= connection->deadline) {
            ConnectionFail(
                connection,
                "timed out after %u s while %s (%llu bytes received)",
                connection->timeout_ms / 1000,
                connection->phase ? connection->phase : "waiting",
                (unsigned long long)connection->bytes_received
            );
            return false;
        }

        // Short waits, to notice a cancel
        u64 remaining = (u64)((connection->deadline - now) * 1000000.0);
        if (remaining > 200000) {
            remaining = 200000;
        }

        fd_set set;
        FD_ZERO(&set);
        FD_SET(socket, &set);
        fd_set exception_set;
        FD_ZERO(&exception_set);
        FD_SET(socket, &exception_set);
        timeval timeout;
        timeout.tv_sec = (long)(remaining / 1000000);
        timeout.tv_usec = (long)(remaining % 1000000);
        int ready = select(0, kind == WIN32_SOCKET_WAIT_READ ? &set : nullptr, kind != WIN32_SOCKET_WAIT_READ ? &set : nullptr, kind == WIN32_SOCKET_WAIT_CONNECT ? &exception_set : nullptr, &timeout);
        if (ready < 0) {
            ConnectionFail(connection, "waiting on the socket failed: %S", Win32SocketErrorText(connection->arena, WSAGetLastError()));
            return false;
        }

        if (ready > 0) {
            if (kind == WIN32_SOCKET_WAIT_CONNECT && FD_ISSET(socket, &exception_set)) {
                if (out_connect_failed) {
                    *out_connect_failed = true;
                }

                return false;
            }

            return true;
        }
    }
}

static bool Win32SocketSendAll(Connection* connection, const u8* data, u64 count) {
    while (count > 0) {
        if (!Win32SocketWaitFor(connection, WIN32_SOCKET_WAIT_WRITE)) {
            return false;
        }

        int sent = send(connection->platform->socket, (const char*)data, (int)(count > 1 << 20 ? 1 << 20 : count), 0);
        if (sent == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                continue;
            }

            ConnectionFail(connection, "send failed: %S", Win32SocketErrorText(connection->arena, error));
            return false;
        }

        data += sent;
        count -= (u64)sent;
    }

    return true;
}

static i64 Win32SocketReceiveSome(Connection* connection, ConnectionBuffer* buffer) {
    if (!Win32SocketWaitFor(connection, WIN32_SOCKET_WAIT_READ)) {
        return -1;
    }

    ConnectionBufferReserve(buffer, 65536);
    int received = recv(connection->platform->socket, (char*)buffer->data + buffer->count, (int)(buffer->capacity - buffer->count), 0);
    if (received == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            return Win32SocketReceiveSome(connection, buffer);
        }

        ConnectionFail(connection, "receive failed: %S", Win32SocketErrorText(connection->arena, error));
        return -1;
    }

    buffer->count += (u64)received;
    connection->bytes_received += (u64)received;

    return received;
}

static void Win32ConnectionPrepare(Connection* connection) {
    if (!connection->platform) {
        connection->platform = ArenaPush(connection->arena, ConnectionPlatform);
        connection->platform->socket = INVALID_SOCKET;
        connection->platform->encrypted.arena = connection->arena;
        connection->platform->plaintext.arena = connection->arena;
    }
}

// Copies `host` into `out_text` with a zero after it, without the brackets of an IPv6 address like [::1]
static void Win32HostText(String8 host, char* out_text, u64 capacity) {
    u64 host_length = host.count < capacity - 1 ? host.count : capacity - 1;
    MemCpy(out_text, host.ptr, host_length);
    out_text[host_length] = 0;
    if (host_length > 2 && out_text[0] == '[' && out_text[host_length - 1] == ']') {
        memmove(out_text, out_text + 1, host_length - 2);
        out_text[host_length - 2] = 0;
    }
}

bool ConnectionOpen(Connection* connection, String8 host, u16 port, u64* out_dns_micros) {
    Win32WinsockStartup();
    Win32ConnectionPrepare(connection);

    char host_text[512];
    Win32HostText(host, host_text, sizeof(host_text));

    String8 port_text = StringFormat(connection->arena, "%u", (u32)port);

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* results = nullptr;
    f64 resolve_start = Platform::GetAbsoluteTime();
    int resolve_status = getaddrinfo(host_text, (const char*)port_text.ptr, &hints, &results);
    *out_dns_micros = (u64)((Platform::GetAbsoluteTime() - resolve_start) * 1000000.0);
    if (resolve_status != 0 || !results) {
        ConnectionFail(connection, "could not resolve %s (error %d)", host_text, resolve_status);
        return false;
    }

    // Start with IPv4
    // localhost often has both, and a proxy (Docker, WSL) may accept on one family
    // without forwarding anywhere
    SOCKET pending[FD_SETSIZE];
    u32 pending_count = 0;
    bool has_ipv4 = false;
    for (addrinfo* candidate = results; candidate; candidate = candidate->ai_next) {
        has_ipv4 = has_ipv4 || candidate->ai_family == AF_INET;
    }

    bool ipv6_started = !has_ipv4;
    f64 ipv6_start = Platform::GetAbsoluteTime() + 0.2;
    auto start_family = [&](int family) {
        for (addrinfo* candidate = results; candidate && pending_count < FD_SETSIZE; candidate = candidate->ai_next) {
            if (candidate->ai_family != family) {
                continue;
            }
            SOCKET candidate_socket = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
            if (candidate_socket == INVALID_SOCKET) {
                continue;
            }
            u_long non_blocking = 1;
            ioctlsocket(candidate_socket, FIONBIO, &non_blocking);
            int connect_status = connect(candidate_socket, candidate->ai_addr, (int)candidate->ai_addrlen);
            if (connect_status == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
                closesocket(candidate_socket);
                continue;
            }
            pending[pending_count++] = candidate_socket;
        }
    };

    start_family(AF_INET);
    if (ipv6_started) {
        start_family(AF_INET6);
    }

    SOCKET connected_socket = INVALID_SOCKET;
    int last_connect_error = 0;
    while ((pending_count > 0 || !ipv6_started) && connected_socket == INVALID_SOCKET) {
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

        u64 remaining = (u64)(remaining_seconds * 1000000.0);
        if (remaining > 200000) {
            remaining = 200000;
        }

        fd_set write_set;
        fd_set exception_set;
        FD_ZERO(&write_set);
        FD_ZERO(&exception_set);
        for (u32 index = 0; index < pending_count; index++) {
            FD_SET(pending[index], &write_set);
            FD_SET(pending[index], &exception_set);
        }

        timeval timeout;
        timeout.tv_sec = (long)(remaining / 1000000);
        timeout.tv_usec = (long)(remaining % 1000000);
        int ready = select(0, nullptr, &write_set, &exception_set, &timeout);
        if (ready < 0) {
            ConnectionFail(connection, "waiting on the socket failed: %S", Win32SocketErrorText(connection->arena, WSAGetLastError()));
            break;
        }

        for (u32 index = 0; index < pending_count;) {
            SOCKET candidate_socket = pending[index];
            if (FD_ISSET(candidate_socket, &write_set) || FD_ISSET(candidate_socket, &exception_set)) {
                int socket_error = 0;
                int error_length = sizeof(socket_error);
                getsockopt(candidate_socket, SOL_SOCKET, SO_ERROR, (char*)&socket_error, &error_length);
                if (socket_error == 0 && FD_ISSET(candidate_socket, &write_set) && connected_socket == INVALID_SOCKET) {
                    connected_socket = candidate_socket;
                    pending[index] = pending[--pending_count];
                    continue;
                }

                if (socket_error != 0) {
                    last_connect_error = socket_error;
                }

                closesocket(candidate_socket);
                pending[index] = pending[--pending_count];
                continue;
            }

            index++;
        }
    }

    for (u32 index = 0; index < pending_count; index++) {
        closesocket(pending[index]);
    }

    freeaddrinfo(results);

    if (connected_socket == INVALID_SOCKET) {
        if (connection->error.count == 0 && last_connect_error != 0) {
            ConnectionFail(connection, "could not connect to %s:%u: %S", host_text, port, Win32SocketErrorText(connection->arena, last_connect_error));
        } else if (connection->error.count == 0) {
            ConnectionFail(connection, "could not connect to %s:%u", host_text, port);
        }
        return false;
    }

    int no_delay = 1;
    setsockopt(connected_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&no_delay, sizeof(no_delay));
    connection->platform->socket = connected_socket;

    return true;
}

// Empty when the certificate verified, otherwise why it did not
static String8 Win32VerifyCertificate(Connection* connection, const wchar_t* host_wide) {
    PCCERT_CONTEXT certificate = nullptr;
    SECURITY_STATUS status = QueryContextAttributesW(&connection->platform->context, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &certificate);
    if (status != SEC_E_OK || !certificate) {
        return StringFormat(connection->arena, "the server sent no certificate to check (0x%08X)", (u32)status);
    }

    CERT_CHAIN_PARA chain_parameters = {};
    chain_parameters.cbSize = sizeof(chain_parameters);
    LPCSTR server_auth = szOID_PKIX_KP_SERVER_AUTH;
    chain_parameters.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
    chain_parameters.RequestedUsage.Usage.cUsageIdentifier = 1;
    chain_parameters.RequestedUsage.Usage.rgpszUsageIdentifier = (LPSTR*)&server_auth;

    PCCERT_CHAIN_CONTEXT chain = nullptr;
    String8 reason = String8{};
    BOOL built_result =
        CertGetCertificateChain(nullptr, certificate, nullptr, certificate->hCertStore, &chain_parameters, CERT_CHAIN_CACHE_END_CERT | CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT, nullptr, &chain);

    if (!built_result) {
        reason = StringFormat(connection->arena, "the certificate chain could not be built (0x%08X)", (u32)GetLastError());
    } else {
        SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl_parameters = {};
        ssl_parameters.cbSize = sizeof(ssl_parameters);
        ssl_parameters.dwAuthType = AUTHTYPE_SERVER;
        ssl_parameters.pwszServerName = (WCHAR*)host_wide;

        CERT_CHAIN_POLICY_PARA policy_parameters = {};
        policy_parameters.cbSize = sizeof(policy_parameters);

        // An unreachable revocation list would warn on every offline machine
        // a revoked certificate still fails
        policy_parameters.dwFlags = CERT_CHAIN_POLICY_IGNORE_ALL_REV_UNKNOWN_FLAGS;
        policy_parameters.pvExtraPolicyPara = &ssl_parameters;

        CERT_CHAIN_POLICY_STATUS policy_status = {};
        policy_status.cbSize = sizeof(policy_status);
        if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain, &policy_parameters, &policy_status)) {
            reason = S("the certificate could not be checked");
        } else if (policy_status.dwError != 0) {
            // CERT_E_ codes are negative HRESULTs
            const char* known = nullptr;
            switch ((LONG)policy_status.dwError) {
            case CERT_E_UNTRUSTEDROOT:      known = "signed by an authority this machine does not trust (self-signed?)"; break;
            case CERT_E_UNTRUSTEDTESTROOT:  known = "chains to a test root"; break;
            case CERT_E_EXPIRED:            known = "expired, or not valid yet"; break;
            case CERT_E_CN_NO_MATCH:        known = "issued for a different host name"; break;
            case CERT_E_WRONG_USAGE:        known = "not valid for server authentication"; break;
            case CERT_E_CHAINING:           known = "chain incomplete"; break;
            case CRYPT_E_REVOKED:           known = "revoked"; break;
            case CERT_E_REVOCATION_FAILURE: known = "revocation status could not be checked"; break;
            case TRUST_E_CERT_SIGNATURE:    known = "signature is not valid"; break;
            case CERT_E_PURPOSE:            known = "not valid for this purpose"; break;
            case CERT_E_ROLE:               known = "an issuer in the chain is not a certificate authority"; break;
            default:                        break;
            }

            if (known) {
                reason = StringFormat(connection->arena, "certificate %s (0x%08X)", known, (u32)policy_status.dwError);
            } else {
                reason = StringFormat(connection->arena, "certificate rejected (0x%08X)", (u32)policy_status.dwError);
            }
        }
    }

    if (chain) {
        CertFreeCertificateChain(chain);
    }

    CertFreeCertificateContext(certificate);

    return reason;
}

bool ConnectionStartTls(Connection* connection, String8 host, String8 alpn, bool skip_verify) {
    ConnectionPlatform* platform = connection->platform;
    SCHANNEL_CRED credential_options = {};
    credential_options.dwVersion = SCHANNEL_CRED_VERSION;
    credential_options.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_3_CLIENT;

    // Schannel doesn't judge the certificate, so a self-signed one can't fail the handshake; it is checked after
    credential_options.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO | SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_SERVERNAME_CHECK | SCH_CRED_IGNORE_NO_REVOCATION_CHECK;

    SECURITY_STATUS status = AcquireCredentialsHandleW(nullptr, (SEC_WCHAR*)UNISP_NAME_W, SECPKG_CRED_OUTBOUND, nullptr, &credential_options, nullptr, nullptr, &platform->credentials, nullptr);
    if (status != SEC_E_OK) {
        ConnectionFail(connection, "AcquireCredentialsHandle failed (0x%08X)", (u32)status);
        return false;
    }

    platform->has_credentials = true;

    wchar_t host_wide[512];
    int wide_length = MultiByteToWideChar(CP_UTF8, 0, host.str, (int)host.count, host_wide, 511);
    host_wide[wide_length < 0 ? 0 : wide_length] = 0;

    u8 alpn_storage[64] = {};
    u64 alpn_count = alpn.count < 32 ? alpn.count : 32;
    SEC_APPLICATION_PROTOCOLS* protocols = (SEC_APPLICATION_PROTOCOLS*)alpn_storage;
    SEC_APPLICATION_PROTOCOL_LIST* protocol_list = &protocols->ProtocolLists[0];
    protocol_list->ProtoNegoExt = SecApplicationProtocolNegotiationExt_ALPN;
    protocol_list->ProtocolListSize = (unsigned short)(1 + alpn_count);
    protocol_list->ProtocolList[0] = (unsigned char)alpn_count;
    MemCpy(&protocol_list->ProtocolList[1], alpn.ptr, alpn_count);
    protocols->ProtocolListsSize = (unsigned long)(FIELD_OFFSET(SEC_APPLICATION_PROTOCOL_LIST, ProtocolList) + 1 + alpn_count);
    u32 alpn_size = (u32)(FIELD_OFFSET(SEC_APPLICATION_PROTOCOLS, ProtocolLists) + protocols->ProtocolListsSize);

    DWORD request_flags =
        ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION;
    DWORD context_flags = 0;
    bool first_call = true;
    for (;;) {
        SecBuffer in_buffers[3] = {};
        u32 in_count = 0;
        if (first_call) {
            if (alpn_count > 0) {
                in_buffers[in_count].BufferType = SECBUFFER_APPLICATION_PROTOCOLS;
                in_buffers[in_count].cbBuffer = alpn_size;
                in_buffers[in_count].pvBuffer = alpn_storage;
                in_count++;
            }
        } else {
            in_buffers[0].BufferType = SECBUFFER_TOKEN;
            in_buffers[0].cbBuffer = (unsigned long)platform->encrypted.count;
            in_buffers[0].pvBuffer = platform->encrypted.data;
            in_buffers[1].BufferType = SECBUFFER_EMPTY;
            in_count = 2;
        }
        SecBufferDesc in_desc = {SECBUFFER_VERSION, in_count, in_buffers};
        SecBuffer out_buffer = {};
        out_buffer.BufferType = SECBUFFER_TOKEN;
        SecBufferDesc out_desc = {SECBUFFER_VERSION, 1, &out_buffer};

        status = InitializeSecurityContextW(
            &platform->credentials,
            first_call ? nullptr : &platform->context,
            host_wide,
            request_flags,
            0,
            0,
            in_count > 0 ? &in_desc : nullptr,
            0,
            first_call ? &platform->context : nullptr,
            &out_desc,
            &context_flags,
            nullptr
        );
        if (first_call && status != SEC_E_INCOMPLETE_MESSAGE) {
            platform->has_context = true;
        }
        first_call = false;

        if (status == SEC_E_INCOMPLETE_MESSAGE) {
            i64 received = Win32SocketReceiveSome(connection, &platform->encrypted);
            if (received <= 0) {
                if (received == 0) {
                    ConnectionFail(connection, "connection closed during TLS handshake");
                }
                return false;
            }
            continue;
        }

        if (out_buffer.pvBuffer && out_buffer.cbBuffer) {
            bool sent = Win32SocketSendAll(connection, (const u8*)out_buffer.pvBuffer, out_buffer.cbBuffer);
            FreeContextBuffer(out_buffer.pvBuffer);
            if (!sent) {
                return false;
            }
        }

        if (status != SEC_E_OK && status != SEC_I_CONTINUE_NEEDED) {
            // Schannel still raises these for a certificate it cannot parse
            const char* reason = "TLS handshake failed";
            if (status == SEC_E_UNTRUSTED_ROOT || status == CERT_E_UNTRUSTEDROOT) {
                reason = "the certificate is not trusted (self-signed?)";
            } else if (status == SEC_E_CERT_EXPIRED || status == CERT_E_EXPIRED) {
                reason = "the certificate has expired";
            } else if (status == SEC_E_WRONG_PRINCIPAL || status == CERT_E_CN_NO_MATCH) {
                reason = "the certificate does not match the host name";
            } else if (status == SEC_E_ILLEGAL_MESSAGE) {
                reason = "the TLS handshake was rejected by the server (is this a TLS port?)";
            } else if (status == SEC_E_ALGORITHM_MISMATCH) {
                reason = "no TLS version or cipher in common with the server";
            }
            ConnectionFail(connection, "%s (0x%08X)", reason, (u32)status);
            return false;
        }

        if (in_buffers[1].BufferType == SECBUFFER_EXTRA && in_buffers[1].cbBuffer > 0) {
            ConnectionBufferConsume(&platform->encrypted, platform->encrypted.count - in_buffers[1].cbBuffer);
        } else {
            platform->encrypted.count = 0;
        }

        if (status == SEC_E_OK) {
            break;
        }

        if (platform->encrypted.count == 0) {
            i64 received = Win32SocketReceiveSome(connection, &platform->encrypted);
            if (received <= 0) {
                if (received == 0) {
                    ConnectionFail(connection, "connection closed during TLS handshake");
                }
                return false;
            }
        }
    }

    if (alpn_count > 0) {
        SecPkgContext_ApplicationProtocol negotiated = {};
        status = QueryContextAttributesW(&platform->context, SECPKG_ATTR_APPLICATION_PROTOCOL, &negotiated);
        if (status != SEC_E_OK || negotiated.ProtoNegoStatus != SecApplicationProtocolNegotiationStatus_Success || negotiated.ProtocolIdSize != alpn_count ||
            memcmp(negotiated.ProtocolId, alpn.ptr, alpn_count) != 0) {
            ConnectionFail(connection, "the server did not agree to %S over TLS (ALPN)", alpn);
            return false;
        }
    }

    status = QueryContextAttributesW(&platform->context, SECPKG_ATTR_STREAM_SIZES, &platform->sizes);
    if (status != SEC_E_OK) {
        ConnectionFail(connection, "QueryContextAttributes(stream sizes) failed (0x%08X)", (u32)status);
        return false;
    }

    platform->send_scratch = ArenaPushArrayNoZero(connection->arena, u8, (u64)platform->sizes.cbHeader + platform->sizes.cbMaximumMessage + platform->sizes.cbTrailer);
    platform->tls = true;

    if (!skip_verify) {
        String8 unverified = Win32VerifyCertificate(connection, host_wide);
        if (unverified.count > 0) {
            connection->tls_warning = StringFormat(connection->arena, "TLS not verified: %S", unverified);
        }
    }

    return true;
}

bool ConnectionSend(Connection* connection, const u8* data, u64 count) {
    ConnectionPlatform* platform = connection->platform;
    if (!platform->tls) {
        return Win32SocketSendAll(connection, data, count);
    }
    while (count > 0) {
        u64 chunk = count < platform->sizes.cbMaximumMessage ? count : platform->sizes.cbMaximumMessage;
        u8* scratch = platform->send_scratch;
        MemCpy(scratch + platform->sizes.cbHeader, data, chunk);
        SecBuffer buffers[4] = {};
        buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
        buffers[0].cbBuffer = platform->sizes.cbHeader;
        buffers[0].pvBuffer = scratch;
        buffers[1].BufferType = SECBUFFER_DATA;
        buffers[1].cbBuffer = (unsigned long)chunk;
        buffers[1].pvBuffer = scratch + platform->sizes.cbHeader;
        buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
        buffers[2].cbBuffer = platform->sizes.cbTrailer;
        buffers[2].pvBuffer = scratch + platform->sizes.cbHeader + chunk;
        buffers[3].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc desc = {SECBUFFER_VERSION, 4, buffers};
        SECURITY_STATUS status = EncryptMessage(&platform->context, 0, &desc, 0);
        if (status != SEC_E_OK) {
            ConnectionFail(connection, "EncryptMessage failed (0x%08X)", (u32)status);
            return false;
        }
        if (!Win32SocketSendAll(connection, scratch, (u64)buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer)) {
            return false;
        }
        data += chunk;
        count -= chunk;
    }
    return true;
}

i64 ConnectionReceive(Connection* connection, const u8** out_data) {
    ConnectionPlatform* platform = connection->platform;
    if (!platform->tls) {
        platform->plaintext.count = 0;
        i64 received = Win32SocketReceiveSome(connection, &platform->plaintext);
        *out_data = platform->plaintext.data;
        return received;
    }

    platform->plaintext.count = 0;
    for (;;) {
        if (platform->encrypted.count > 0) {
            SecBuffer buffers[4] = {};
            buffers[0].BufferType = SECBUFFER_DATA;
            buffers[0].cbBuffer = (unsigned long)platform->encrypted.count;
            buffers[0].pvBuffer = platform->encrypted.data;
            buffers[1].BufferType = SECBUFFER_EMPTY;
            buffers[2].BufferType = SECBUFFER_EMPTY;
            buffers[3].BufferType = SECBUFFER_EMPTY;
            SecBufferDesc desc = {SECBUFFER_VERSION, 4, buffers};
            SECURITY_STATUS status = DecryptMessage(&platform->context, &desc, 0, nullptr);
            if (status == SEC_E_OK || status == SEC_I_RENEGOTIATE) {
                u64 extra = 0;
                for (u32 index = 0; index < 4; index++) {
                    if (buffers[index].BufferType == SECBUFFER_DATA) {
                        ConnectionBufferAppend(&platform->plaintext, (const u8*)buffers[index].pvBuffer, buffers[index].cbBuffer);
                    } else if (buffers[index].BufferType == SECBUFFER_EXTRA) {
                        extra = buffers[index].cbBuffer;
                    }
                }

                ConnectionBufferConsume(&platform->encrypted, platform->encrypted.count - extra);
                if (status == SEC_I_RENEGOTIATE) {
                    ConnectionFail(connection, "server requested TLS renegotiation, which is not supported");
                    return -1;
                }

                if (platform->plaintext.count > 0) {
                    *out_data = platform->plaintext.data;
                    return (i64)platform->plaintext.count;
                }

                continue;
            }

            if (status == SEC_I_CONTEXT_EXPIRED) {
                platform->encrypted.count = 0;
                return 0;
            }

            if (status != SEC_E_INCOMPLETE_MESSAGE) {
                ConnectionFail(connection, "DecryptMessage failed (0x%08X)", (u32)status);
                return -1;
            }
        }

        i64 received = Win32SocketReceiveSome(connection, &platform->encrypted);
        if (received <= 0) {
            return received;
        }
    }
}

i32 ConnectionReadableWithin(Connection* connection, u64 micros) {
    ConnectionPlatform* platform = connection->platform;

    // Bytes TLS has buffered are no longer on the socket
    if (platform->tls && platform->encrypted.count > 0) {
        return 1;
    }

    fd_set set;
    FD_ZERO(&set);
    FD_SET(platform->socket, &set);
    timeval timeout;
    timeout.tv_sec = (long)(micros / 1000000);
    timeout.tv_usec = (long)(micros % 1000000);

    int ready = select(0, &set, nullptr, nullptr, &timeout);
    if (ready < 0) {
        ConnectionFail(connection, "waiting on the socket failed: %S", Win32SocketErrorText(connection->arena, WSAGetLastError()));
        return -1;
    }

    return ready > 0 ? 1 : 0;
}

bool ConnectionIsOpen(Connection* connection) {
    return connection->platform && connection->platform->socket != INVALID_SOCKET;
}

void ConnectionClose(Connection* connection) {
    ConnectionPlatform* platform = connection->platform;
    if (!platform) {
        return;
    }

    if (platform->has_context) {
        DeleteSecurityContext(&platform->context);
        platform->has_context = false;
    }

    if (platform->has_credentials) {
        FreeCredentialsHandle(&platform->credentials);
        platform->has_credentials = false;
    }

    if (platform->socket != INVALID_SOCKET) {
        closesocket(platform->socket);
    }

    platform->socket = INVALID_SOCKET;
}

#define KRAFT_LISTENER_MAX_SOCKETS 8

struct Listener {
    ArenaAllocator* arena;
    SOCKET sockets[KRAFT_LISTENER_MAX_SOCKETS];
    u32 socket_count;
    u16 port;
};

static SOCKET Win32ListenOn(addrinfo* address, u16 port, int* out_error) {
    SOCKET listening = ::socket(address->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if (listening == INVALID_SOCKET) {
        *out_error = WSAGetLastError();
        return INVALID_SOCKET;
    }

    // Nobody else can take the port while it is listened on
    BOOL exclusive = TRUE;
    setsockopt(listening, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive));

    sockaddr_storage bound_address = {};
    MemCpy(&bound_address, address->ai_addr, address->ai_addrlen);
    if (address->ai_family == AF_INET) {
        ((sockaddr_in*)&bound_address)->sin_port = htons(port);
    } else {
        ((sockaddr_in6*)&bound_address)->sin6_port = htons(port);
    }

    if (bind(listening, (sockaddr*)&bound_address, (int)address->ai_addrlen) == SOCKET_ERROR || listen(listening, SOMAXCONN) == SOCKET_ERROR) {
        *out_error = WSAGetLastError();
        closesocket(listening);
        return INVALID_SOCKET;
    }

    u_long non_blocking = 1;
    ioctlsocket(listening, FIONBIO, &non_blocking);

    return listening;
}

Listener* ListenerStart(String8 host, u16 port, ListenerStartStatus* out_status) {
    ListenerStartStatus ignored_status;
    if (!out_status) {
        out_status = &ignored_status;
    }

    *out_status = LISTENER_START_STATUS_FAILED;
    Win32WinsockStartup();

    char host_text[512];
    Win32HostText(host, host_text, sizeof(host_text));
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

            SOCKET listening = Win32ListenOn(address, listener->port, &last_error);
            if (listening == INVALID_SOCKET) {
                continue;
            }

            if (listener->port == 0) {
                sockaddr_storage bound_address = {};
                int size = sizeof(bound_address);
                getsockname(listening, (sockaddr*)&bound_address, &size);
                listener->port = ntohs(bound_address.ss_family == AF_INET ? ((sockaddr_in*)&bound_address)->sin_port : ((sockaddr_in6*)&bound_address)->sin6_port);
            }

            listener->sockets[listener->socket_count++] = listening;
        }
    }

    freeaddrinfo(results);

    if (listener->socket_count == 0) {
        *out_status = last_error == WSAEADDRINUSE || last_error == WSAEACCES ? LISTENER_START_STATUS_PORT_TAKEN : LISTENER_START_STATUS_FAILED;
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
    fd_set readable;
    FD_ZERO(&readable);
    for (u32 index = 0; index < listener->socket_count; index++) {
        FD_SET(listener->sockets[index], &readable);
    }

    timeval timeout;
    timeout.tv_sec = (long)(timeout_ms / 1000);
    timeout.tv_usec = (long)((timeout_ms % 1000) * 1000);
    int ready = select(0, &readable, nullptr, nullptr, &timeout);
    if (ready < 0) {
        return -1;
    }

    for (u32 index = 0; index < listener->socket_count && ready > 0; index++) {
        if (!FD_ISSET(listener->sockets[index], &readable)) {
            continue;
        }

        // The peer may have given up between the two calls
        SOCKET accepted = accept(listener->sockets[index], nullptr, nullptr);
        if (accepted == INVALID_SOCKET) {
            continue;
        }

        u_long non_blocking = 1;
        ioctlsocket(accepted, FIONBIO, &non_blocking);
        Win32ConnectionPrepare(out_connection);
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
        closesocket(listener->sockets[index]);
    }

    DestroyArena(listener->arena);
}

} // namespace kraft::net
