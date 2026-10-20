/* Socket callbacks over Winsock 2 and Schannel.
 *
 * The TLS client follows the SSPI sequence Microsoft documents for Schannel:
 * AcquireCredentialsHandle(), then InitializeSecurityContext() until the
 * handshake is done, EncryptMessage() and DecryptMessage() for the data, and
 * ApplyControlToken(SCHANNEL_SHUTDOWN) with one more
 * InitializeSecurityContext() for the close_notify alert.
 *
 * Schannel does not do any I/O itself. It takes what has been received and
 * says what it needs next, so this file keeps the bytes received but not yet
 * used in socket_context_t#recv_buf; see sock_cb_win32.h. */

#include "sock_cb_win32.h"

#include <ws2tcpip.h>
#include <schannel.h>
#include <wincrypt.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The largest TLS record: a 5-byte header and at most 2^14 + 2048 bytes
   after it (RFC 5246, 6.2.3; TLS 1.3 allows less). recv_buf always starts at
   a record boundary, so it always has room for the rest of the record it
   starts with. */
#define TLS_MAX_RECORD_SIZE (5 + 16384 + 2048)

/* The first byte of a TLS record's header, for an alert. */
#define TLS_CONTENT_TYPE_ALERT 21

/* Requested of InitializeSecurityContext() for every call on a context.
   ALLOCATE_MEMORY has Schannel allocate the output tokens, which are freed
   with FreeContextBuffer(). EXTENDED_ERROR has it write a TLS alert when the
   handshake fails, to be sent to the server like any other output. */
static const ULONG isc_flags =
    ISC_REQ_SEQUENCE_DETECT |
    ISC_REQ_REPLAY_DETECT |
    ISC_REQ_CONFIDENTIALITY |
    ISC_REQ_EXTENDED_ERROR |
    ISC_REQ_ALLOCATE_MEMORY |
    ISC_REQ_STREAM;

typedef enum {
    READ_OK,
    READ_TIMEOUT,
    READ_EOF,
    READ_ERROR
} read_result_t;

typedef enum {
    DECRYPT_OK,
    DECRYPT_INCOMPLETE,
    DECRYPT_CLOSED,
    DECRYPT_ERROR
} decrypt_result_t;

/* Prints what failed with a Winsock, Win32, SSPI or certificate error code,
   and the system's text for it where there is one. */
static void print_error(const char* what, unsigned long code)
{
    char text[256];
    DWORD len = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, code, 0, text, sizeof(text), NULL);
    while (len > 0 &&
            (text[len - 1] == '\r' || text[len - 1] == '\n' ||
             text[len - 1] == ' ' || text[len - 1] == '.')) {
        --len;
    }
    text[len] = '\0';
    printf("%s: 0x%08lx%s%s\n", what, code, len > 0 ? " " : "", text);
}

static void set_buffer(SecBuffer* buffer, unsigned long type, void* data,
        size_t size)
{
    buffer->BufferType = type;
    buffer->pvBuffer = data;
    buffer->cbBuffer = (unsigned long)size;
}

static void set_buffer_desc(SecBufferDesc* desc, SecBuffer* buffers,
        unsigned long count)
{
    desc->ulVersion = SECBUFFER_VERSION;
    desc->cBuffers = count;
    desc->pBuffers = buffers;
}

void sock_cb_init_context(
        socket_context_t* ctx,
        unsigned int to_recv,
        unsigned int to_send)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->to_recv = to_recv;
    ctx->to_send = to_send;
    ctx->socket = INVALID_SOCKET;
}

/* Releases everything the connection holds, whatever state it is in, and
   leaves the context ready for the next sock_cb_connect(). */
static void teardown(socket_context_t* ctx)
{
    if (ctx->socket != INVALID_SOCKET) {
        closesocket(ctx->socket);
        ctx->socket = INVALID_SOCKET;
    }
    if (ctx->has_ctxt) {
        DeleteSecurityContext(&ctx->ctxt);
        ctx->has_ctxt = 0;
    }
    if (ctx->has_cred) {
        FreeCredentialsHandle(&ctx->cred);
        ctx->has_cred = 0;
    }
    free(ctx->recv_buf);
    ctx->recv_buf = NULL;
    free(ctx->send_buf);
    ctx->send_buf = NULL;
    ctx->recv_len = 0;
    ctx->record_len = 0;
    ctx->plain = NULL;
    ctx->plain_len = 0;
    ctx->peer_closed = 0;
}

static SOCKET tcp_connect(const char* host, uint16_t port,
        unsigned int to_send)
{
    char service[6];
    snprintf(service, sizeof(service), "%u", (unsigned int)port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo* addrs = NULL;
    int err = getaddrinfo(host, service, &hints, &addrs);
    if (err != 0) {
        printf("failed to get host %s.\n", host);
        print_error("getaddrinfo", (unsigned long)err);
        return INVALID_SOCKET;
    }

    /* The first address that accepts, IPv6 or IPv4. */
    SOCKET sock = INVALID_SOCKET;
    int last_error = 0;
    for (struct addrinfo* addr = addrs; addr != NULL; addr = addr->ai_next) {
        sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            last_error = WSAGetLastError();
            continue;
        }
        if (connect(sock, addr->ai_addr, (int)addr->ai_addrlen) == 0) {
            break;
        }
        last_error = WSAGetLastError();
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    freeaddrinfo(addrs);
    if (sock == INVALID_SOCKET) {
        printf("failed to connect socket.\n");
        print_error("connect", (unsigned long)last_error);
        return INVALID_SOCKET;
    }

    /* A send that cannot complete within the timeout fails, and the
       connection is given up. There is no SO_RCVTIMEO: read_some() waits
       for the receive timeout itself. */
    if (to_send > 0) {
        DWORD ms = to_send > MAXDWORD / 1000 ? MAXDWORD : to_send * 1000;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
                    (const char*)&ms, sizeof(ms)) != 0) {
            print_error("setsockopt(SO_SNDTIMEO)", WSAGetLastError());
            closesocket(sock);
            return INVALID_SOCKET;
        }
    }
    return sock;
}

static int send_all(SOCKET sock, const char* data, size_t length)
{
    while (length > 0) {
        int chunk = length > INT_MAX ? INT_MAX : (int)length;
        int sent = send(sock, data, chunk, 0);
        if (sent == SOCKET_ERROR) {
            print_error("send failed", WSAGetLastError());
            return -1;
        }
        data += sent;
        length -= (size_t)sent;
    }
    return 0;
}

/* Appends whatever the peer has sent to recv_buf, waiting up to to_recv for
 * something to arrive.
 *
 * The wait is select()'s, rather than a recv() ended by SO_RCVTIMEO: Winsock
 * documents a socket on which a receive has timed out as being in an
 * indeterminate state, and liable to lose data, and the MQTT task goes on
 * using its socket after every idle timeout. A recv() that only starts once
 * select() has seen data, the end of the stream or an error never blocks, so
 * a timeout here leaves the socket as it was, and the TLS stream with it. */
static read_result_t read_some(socket_context_t* ctx)
{
    size_t room = TLS_MAX_RECORD_SIZE - ctx->recv_len;
    if (room == 0) {
        printf("TLS record larger than %d bytes.\n", TLS_MAX_RECORD_SIZE);
        return READ_ERROR;
    }

    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(ctx->socket, &readable);
    struct timeval timeout;
    timeout.tv_sec = ctx->to_recv > LONG_MAX ? LONG_MAX : (long)ctx->to_recv;
    timeout.tv_usec = 0;
    /* Winsock ignores the first argument. */
    int ready = select(0, &readable, NULL, NULL,
            ctx->to_recv > 0 ? &timeout : NULL);
    if (ready == SOCKET_ERROR) {
        print_error("select failed", WSAGetLastError());
        return READ_ERROR;
    }
    if (ready == 0) {
        return READ_TIMEOUT;
    }

    int received = recv(ctx->socket, ctx->recv_buf + ctx->recv_len,
            (int)room, 0);
    if (received == SOCKET_ERROR) {
        print_error("recv failed", WSAGetLastError());
        return READ_ERROR;
    }
    if (received == 0) {
        return READ_EOF;
    }
    ctx->recv_len += (size_t)received;
    return READ_OK;
}

/* After InitializeSecurityContext() or DecryptMessage(), a SECBUFFER_EXTRA
   buffer gives the number of bytes at the end of the input that it did not
   use, which are the start of the next message. Keeps those, and only those,
   at the start of recv_buf. */
static void keep_extra(socket_context_t* ctx, const SecBuffer* buffers,
        unsigned long count)
{
    size_t extra = 0;
    for (unsigned long i = 0; i < count; ++i) {
        if (buffers[i].BufferType == SECBUFFER_EXTRA) {
            extra = buffers[i].cbBuffer;
        }
    }
    memmove(ctx->recv_buf, ctx->recv_buf + ctx->recv_len - extra, extra);
    ctx->recv_len = extra;
}

/* Runs the handshake. What the server sends is read into recv_buf, and
   whatever is left in it at the end is the start of the application data. */
static int handshake(socket_context_t* ctx)
{
    for (;;) {
        SecBuffer in[2];
        SecBufferDesc in_desc;
        set_buffer(&in[0], SECBUFFER_TOKEN, ctx->recv_buf, ctx->recv_len);
        set_buffer(&in[1], SECBUFFER_EMPTY, NULL, 0);
        set_buffer_desc(&in_desc, in, 2);

        SecBuffer out[1];
        SecBufferDesc out_desc;
        set_buffer(&out[0], SECBUFFER_TOKEN, NULL, 0);
        set_buffer_desc(&out_desc, out, 1);

        /* The target name is what Schannel sends as SNI. */
        const int first = !ctx->has_ctxt;
        ULONG attrs = 0;
        SECURITY_STATUS status = InitializeSecurityContextA(
                &ctx->cred,
                first ? NULL : &ctx->ctxt,
                (SEC_CHAR*)ctx->host,
                isc_flags,
                0,
                0,
                first ? NULL : &in_desc,
                0,
                &ctx->ctxt,
                &out_desc,
                &attrs,
                NULL);
        if (first && !FAILED(status)) {
            ctx->has_ctxt = 1;
        }

        /* Whatever Schannel has written goes to the server, even when the
           handshake has failed, as it is then an alert saying why. */
        int sent = 0;
        if (out[0].pvBuffer != NULL) {
            if (out[0].cbBuffer > 0) {
                sent = send_all(ctx->socket, out[0].pvBuffer, out[0].cbBuffer);
            }
            FreeContextBuffer(out[0].pvBuffer);
        }

        if (status == SEC_E_INCOMPLETE_MESSAGE) {
            /* Keep all of it, and wait for the rest of the record. */
        } else if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED) {
            if (sent != 0) {
                return -1;
            }
            keep_extra(ctx, in, 2);
            if (status == SEC_E_OK) {
                return 0;
            }
            if (ctx->recv_len > 0) {
                /* The server's next message has arrived already. */
                continue;
            }
        } else if (status == SEC_I_INCOMPLETE_CREDENTIALS) {
            printf("the server asked for a client certificate, "
                    "which this sample does not have.\n");
            return -1;
        } else {
            print_error("TLS handshake failed", (unsigned long)status);
            return -1;
        }

        switch (read_some(ctx)) {
            case READ_OK:
                break;
            case READ_TIMEOUT:
                printf("TLS handshake timed out.\n");
                return -1;
            case READ_EOF:
                printf("the server closed the connection "
                        "during the TLS handshake.\n");
                return -1;
            default:
                return -1;
        }
    }
}

/* Checks the server's certificate: a chain to a root in the Windows root
 * store, the certificate valid for server authentication, issued for the host
 * name connected to, and in date. These are the checks Microsoft documents for
 * validating a server certificate by hand, with CertGetCertificateChain() and
 * CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL).
 *
 * Done here rather than by Schannel, with SCH_CRED_AUTO_CRED_VALIDATION,
 * because wine's Schannel accepts any certificate whatever it is asked:
 * self-signed, expired or issued for another name. Wine's crypt32 does
 * validate, and its root store is the CA bundle of the system it runs on, so
 * this way the certificate is checked on both. Revocation is not checked,
 * which Schannel does only when asked to either. */
static int verify_server_certificate(socket_context_t* ctx)
{
    PCCERT_CONTEXT cert = NULL;
    SECURITY_STATUS status = QueryContextAttributesA(
            &ctx->ctxt, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &cert);
    if (status != SEC_E_OK || cert == NULL) {
        print_error("no server certificate", (unsigned long)status);
        return -1;
    }

    int result = -1;
    PCCERT_CHAIN_CONTEXT chain = NULL;
    WCHAR server_name[256];
    if (MultiByteToWideChar(CP_UTF8, 0, ctx->host, -1, server_name,
                sizeof(server_name) / sizeof(server_name[0])) == 0) {
        print_error("host name", GetLastError());
        goto done;
    }

    /* The certificate's store holds the intermediates the server sent. */
    LPSTR usages[] = { szOID_PKIX_KP_SERVER_AUTH };
    CERT_CHAIN_PARA chain_para;
    memset(&chain_para, 0, sizeof(chain_para));
    chain_para.cbSize = sizeof(chain_para);
    chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
    chain_para.RequestedUsage.Usage.cUsageIdentifier = 1;
    chain_para.RequestedUsage.Usage.rgpszUsageIdentifier = usages;
    if (!CertGetCertificateChain(NULL, cert, NULL, cert->hCertStore,
                &chain_para, 0, NULL, &chain)) {
        print_error("CertGetCertificateChain", GetLastError());
        goto done;
    }

    SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl_para;
    memset(&ssl_para, 0, sizeof(ssl_para));
    ssl_para.cbSize = sizeof(ssl_para);
    ssl_para.dwAuthType = AUTHTYPE_SERVER;
    ssl_para.pwszServerName = server_name;

    CERT_CHAIN_POLICY_PARA policy_para;
    memset(&policy_para, 0, sizeof(policy_para));
    policy_para.cbSize = sizeof(policy_para);
    policy_para.pvExtraPolicyPara = &ssl_para;

    CERT_CHAIN_POLICY_STATUS policy_status;
    memset(&policy_status, 0, sizeof(policy_status));
    policy_status.cbSize = sizeof(policy_status);

    if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain,
                &policy_para, &policy_status)) {
        print_error("CertVerifyCertificateChainPolicy", GetLastError());
        goto done;
    }
    if (policy_status.dwError != 0) {
        printf("server certificate for %s rejected.\n", ctx->host);
        print_error("certificate", policy_status.dwError);
        goto done;
    }
    result = 0;

done:
    if (chain != NULL) {
        CertFreeCertificateChain(chain);
    }
    CertFreeCertificateContext(cert);
    return result;
}

static int tls_connect(socket_context_t* ctx)
{
    /* SCHANNEL_CRED, with which Schannel negotiates TLS 1.2 at most: TLS 1.3
       needs SCH_CREDENTIALS, which older versions of Windows do not have. No
       protocol versions are named, so those the system enables are used,
       which an administrator can configure. Manual validation, since
       verify_server_certificate() does it. */
    SCHANNEL_CRED cred_data;
    memset(&cred_data, 0, sizeof(cred_data));
    cred_data.dwVersion = SCHANNEL_CRED_VERSION;
    cred_data.dwFlags =
        SCH_CRED_MANUAL_CRED_VALIDATION |
        SCH_CRED_NO_DEFAULT_CREDS |
        SCH_USE_STRONG_CRYPTO;
    SECURITY_STATUS status = AcquireCredentialsHandleA(
            NULL,
            (SEC_CHAR*)UNISP_NAME_A,
            SECPKG_CRED_OUTBOUND,
            NULL,
            &cred_data,
            NULL,
            NULL,
            &ctx->cred,
            NULL);
    if (status != SEC_E_OK) {
        print_error("AcquireCredentialsHandle", (unsigned long)status);
        return -1;
    }
    ctx->has_cred = 1;

    ctx->recv_buf = (char*)malloc(TLS_MAX_RECORD_SIZE);
    if (ctx->recv_buf == NULL) {
        printf("failed to allocate the receive buffer.\n");
        return -1;
    }

    if (handshake(ctx) != 0) {
        return -1;
    }
    if (verify_server_certificate(ctx) != 0) {
        return -1;
    }

    status = QueryContextAttributesA(
            &ctx->ctxt, SECPKG_ATTR_STREAM_SIZES, &ctx->sizes);
    if (status != SEC_E_OK) {
        print_error("QueryContextAttributes(STREAM_SIZES)",
                (unsigned long)status);
        return -1;
    }
    ctx->send_buf = (char*)malloc(ctx->sizes.cbHeader +
            ctx->sizes.cbMaximumMessage + ctx->sizes.cbTrailer);
    if (ctx->send_buf == NULL) {
        printf("failed to allocate the send buffer.\n");
        return -1;
    }
    return 0;
}

khc_sock_code_t
    sock_cb_connect(void* sock_ctx, const char* host,
            uint16_t port)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    /* Neither khc nor kii connects again without closing first, but should
       that change, the old connection must not leak. */
    teardown(ctx);
    ctx->recv_timed_out = 0;

    size_t host_len = strlen(host);
    if (host_len >= sizeof(ctx->host)) {
        printf("host name too long: %s\n", host);
        return KHC_SOCK_FAIL;
    }
    memcpy(ctx->host, host, host_len + 1);

    ctx->socket = tcp_connect(ctx->host, port, ctx->to_send);
    if (ctx->socket == INVALID_SOCKET) {
        return KHC_SOCK_FAIL;
    }
    /* Neither khc nor kii closes a connection that failed to open. */
    if (tls_connect(ctx) != 0) {
        teardown(ctx);
        return KHC_SOCK_FAIL;
    }
    return KHC_SOCK_OK;
}

khc_sock_code_t
    sock_cb_send(void* sock_ctx,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    *out_sent_length = 0;
    if (ctx->send_buf == NULL) {
        printf("failed to send: not connected.\n");
        return KHC_SOCK_FAIL;
    }
    if (length == 0) {
        return KHC_SOCK_OK;
    }

    /* One record at most. The caller sends the rest in further calls. */
    size_t data_len = length;
    if (data_len > ctx->sizes.cbMaximumMessage) {
        data_len = ctx->sizes.cbMaximumMessage;
    }
    char* header = ctx->send_buf;
    char* data = header + ctx->sizes.cbHeader;
    char* trailer = data + data_len;
    memcpy(data, buffer, data_len);

    SecBuffer buffers[4];
    SecBufferDesc desc;
    set_buffer(&buffers[0], SECBUFFER_STREAM_HEADER, header,
            ctx->sizes.cbHeader);
    set_buffer(&buffers[1], SECBUFFER_DATA, data, data_len);
    set_buffer(&buffers[2], SECBUFFER_STREAM_TRAILER, trailer,
            ctx->sizes.cbTrailer);
    set_buffer(&buffers[3], SECBUFFER_EMPTY, NULL, 0);
    set_buffer_desc(&desc, buffers, 4);
    SECURITY_STATUS status = EncryptMessage(&ctx->ctxt, 0, &desc, 0);
    if (status != SEC_E_OK) {
        print_error("EncryptMessage", (unsigned long)status);
        return KHC_SOCK_FAIL;
    }

    /* The trailer can come out shorter than cbTrailer. */
    size_t record_len = (size_t)buffers[0].cbBuffer + buffers[1].cbBuffer +
        buffers[2].cbBuffer;
    if (send_all(ctx->socket, header, record_len) != 0) {
        printf("failed to send\n");
        return KHC_SOCK_FAIL;
    }
    *out_sent_length = data_len;
    return KHC_SOCK_OK;
}

/* Forgets the record decrypted last, once all of its plaintext has been
   returned, keeping whatever follows it in recv_buf. */
static void drop_record(socket_context_t* ctx)
{
    memmove(ctx->recv_buf, ctx->recv_buf + ctx->record_len,
            ctx->recv_len - ctx->record_len);
    ctx->recv_len -= ctx->record_len;
    ctx->record_len = 0;
    ctx->plain = NULL;
    ctx->plain_len = 0;
}

/* Decrypts the record at the start of recv_buf, if all of it has arrived. */
static decrypt_result_t decrypt_record(socket_context_t* ctx)
{
    /* The record's content type, from its header, as received. */
    const unsigned char content_type = (unsigned char)ctx->recv_buf[0];

    SecBuffer buffers[4];
    SecBufferDesc desc;
    set_buffer(&buffers[0], SECBUFFER_DATA, ctx->recv_buf, ctx->recv_len);
    set_buffer(&buffers[1], SECBUFFER_EMPTY, NULL, 0);
    set_buffer(&buffers[2], SECBUFFER_EMPTY, NULL, 0);
    set_buffer(&buffers[3], SECBUFFER_EMPTY, NULL, 0);
    set_buffer_desc(&desc, buffers, 4);
    SECURITY_STATUS status = DecryptMessage(&ctx->ctxt, &desc, 0, NULL);
    if (status == SEC_E_INCOMPLETE_MESSAGE) {
        return DECRYPT_INCOMPLETE;
    }
    if (status != SEC_E_OK && status != SEC_I_CONTEXT_EXPIRED &&
            status != SEC_I_RENEGOTIATE) {
        print_error("DecryptMessage", (unsigned long)status);
        return DECRYPT_ERROR;
    }

    /* The plaintext is somewhere inside the record, and the record is
       followed by the EXTRA bytes, if any. */
    size_t extra = 0;
    ctx->plain = NULL;
    ctx->plain_len = 0;
    for (int i = 0; i < 4; ++i) {
        if (buffers[i].BufferType == SECBUFFER_DATA) {
            ctx->plain = (char*)buffers[i].pvBuffer;
            ctx->plain_len = buffers[i].cbBuffer;
        } else if (buffers[i].BufferType == SECBUFFER_EXTRA) {
            extra = buffers[i].cbBuffer;
        }
    }
    ctx->record_len = ctx->recv_len - extra;

    /* close_notify, after which nothing counts. Schannel says so with
       SEC_I_CONTEXT_EXPIRED. Wine's says SEC_E_OK, with no data, as for an
       empty record; but the record was an alert, and any other alert would
       have failed. In TLS 1.3 every record has the application data type in
       its header, but wine does not negotiate TLS 1.3, and Schannel does not
       with SCHANNEL_CRED. */
    if (status == SEC_I_CONTEXT_EXPIRED ||
            (status == SEC_E_OK && ctx->plain_len == 0 &&
             content_type == TLS_CONTENT_TYPE_ALERT)) {
        ctx->plain_len = 0;
        return DECRYPT_CLOSED;
    }
    if (status == SEC_I_RENEGOTIATE) {
        /* A TLS 1.2 server asking for a new handshake, which TLS 1.3 has done
           away with. This sample does not renegotiate, and wine's Schannel
           could not test it if it did: it does not write the ClientHello
           until the server has sent something more. */
        printf("the server asked to renegotiate, "
                "which this sample does not support.\n");
        return DECRYPT_ERROR;
    }
    if (ctx->plain_len == 0) {
        drop_record(ctx);
    }
    return DECRYPT_OK;
}

khc_sock_code_t
    sock_cb_recv(void* sock_ctx,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    *out_actual_length = 0;
    ctx->recv_timed_out = 0;
    if (ctx->recv_buf == NULL) {
        printf("failed to receive: not connected.\n");
        return KHC_SOCK_FAIL;
    }

    for (;;) {
        /* Plaintext left from a record decrypted before. */
        if (ctx->plain_len > 0) {
            size_t len = length_to_read;
            if (len > ctx->plain_len) {
                len = ctx->plain_len;
            }
            memcpy(buffer, ctx->plain, len);
            ctx->plain += len;
            ctx->plain_len -= len;
            if (ctx->plain_len == 0) {
                drop_record(ctx);
            }
            *out_actual_length = len;
            return KHC_SOCK_OK;
        }

        /* The end of the stream, 0 bytes, for this and any later call. */
        if (ctx->peer_closed) {
            return KHC_SOCK_OK;
        }

        if (ctx->recv_len > 0) {
            decrypt_result_t decrypted = decrypt_record(ctx);
            if (decrypted == DECRYPT_OK) {
                continue;
            } else if (decrypted == DECRYPT_CLOSED) {
                ctx->peer_closed = 1;
                continue;
            } else if (decrypted == DECRYPT_ERROR) {
                return KHC_SOCK_FAIL;
            }
            /* DECRYPT_INCOMPLETE: the rest of the record is to come. */
        }

        read_result_t read = read_some(ctx);
        if (read == READ_TIMEOUT) {
            ctx->recv_timed_out = 1;
            return KHC_SOCK_FAIL;
        } else if (read == READ_EOF) {
            /* A server that answers "Connection: close" may end the TCP
               stream without a close_notify, which is an ordinary end of the
               stream, unless it ends in the middle of a record. */
            if (ctx->recv_len == 0) {
                ctx->peer_closed = 1;
                return KHC_SOCK_OK;
            }
            printf("connection closed in the middle of a TLS record.\n");
            return KHC_SOCK_FAIL;
        } else if (read == READ_ERROR) {
            return KHC_SOCK_FAIL;
        }
    }
}

/* sock_cb_recv() returns KHC_SOCK_FAIL both when nothing arrives within
   to_recv and when the connection fails, and says which in recv_timed_out. A
   timeout leaves the connection usable: read_some() never started a recv()
   for it, and any part of a record that did arrive is kept. */
kii_bool_t
    sock_cb_recv_timed_out(void* sock_ctx)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    return ctx->recv_timed_out ? KII_TRUE : KII_FALSE;
}

/* Sends close_notify. Fails only if Schannel cannot make one. Not being able
   to send it is the normal outcome when the peer has closed the connection
   already, as a server answering "Connection: close" does, and harmless when
   the connection is being torn down anyway. */
static int send_close_notify(socket_context_t* ctx)
{
    DWORD type = SCHANNEL_SHUTDOWN;
    SecBuffer in[1];
    SecBufferDesc in_desc;
    set_buffer(&in[0], SECBUFFER_TOKEN, &type, sizeof(type));
    set_buffer_desc(&in_desc, in, 1);
    SECURITY_STATUS status = ApplyControlToken(&ctx->ctxt, &in_desc);
    if (status != SEC_E_OK) {
        print_error("ApplyControlToken(SCHANNEL_SHUTDOWN)",
                (unsigned long)status);
        return -1;
    }

    SecBuffer out[1];
    SecBufferDesc out_desc;
    set_buffer(&out[0], SECBUFFER_TOKEN, NULL, 0);
    set_buffer_desc(&out_desc, out, 1);
    ULONG attrs = 0;
    status = InitializeSecurityContextA(
            &ctx->cred,
            &ctx->ctxt,
            NULL,
            isc_flags,
            0,
            0,
            NULL,
            0,
            &ctx->ctxt,
            &out_desc,
            &attrs,
            NULL);
    if (out[0].pvBuffer != NULL) {
        if (out[0].cbBuffer > 0) {
            /* A few dozen bytes, which one send() takes whole. */
            send(ctx->socket, (const char*)out[0].pvBuffer,
                    (int)out[0].cbBuffer, 0);
        }
        FreeContextBuffer(out[0].pvBuffer);
    }
    if (FAILED(status)) {
        print_error("failed to make close_notify", (unsigned long)status);
        return -1;
    }
    return 0;
}

khc_sock_code_t
    sock_cb_close(void* sock_ctx)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    int ok = 1;
    if (ctx->has_ctxt && ctx->send_buf != NULL) {
        ok = send_close_notify(ctx) == 0;
    }
    teardown(ctx);
    if (!ok) {
        printf("failed to close:\n");
        return KHC_SOCK_FAIL;
    }
    return KHC_SOCK_OK;
}
