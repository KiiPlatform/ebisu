#ifndef EBISU_TESTS_HTTP_SCRIPT_H
#define EBISU_TESTS_HTTP_SCRIPT_H

#include "khc.h"
#include "test_callbacks.h"

#include <sstream>
#include <string>
#include <vector>
#include <string.h>

namespace khct {

inline std::string http_response(uint16_t status, const std::string& body) {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << status << " Status\r\n"
       << "Content-Type: application/json\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "\r\n"
       << body;
    return ss.str();
}

// Plays a scripted HTTP exchange per connection and keeps what each one sent.
// An empty response refuses that connection instead. Running out of script
// fails the connection too, which the test then sees as a transport error.
//
// With respond set, each request is answered with what it returns for the
// request instead, for when the order of the requests is not known ahead.
//
// The callbacks capture this, so an instance must not be copied or moved
// once attached.
struct HttpScript {
    std::vector<std::string> responses;
    std::vector<std::string> requests;
    size_t pos;
    khct::cb::SockCtx ctx;
    std::function<void(size_t)> on_connect;
    std::function<std::string(const std::string& request)> respond;

    HttpScript() : pos(0) {
        ctx.on_connect = [this](void*, const char*, uint16_t) {
            requests.push_back("");
            pos = 0;
            if (on_connect) {
                on_connect(requests.size() - 1);
            }
            if (respond) {
                return KHC_SOCK_OK;
            }
            if (requests.size() > responses.size() || current().empty()) {
                return KHC_SOCK_FAIL;
            }
            return KHC_SOCK_OK;
        };
        ctx.on_send = [this](void*, const char* buffer, size_t length, size_t* out_sent_length) {
            requests.back().append(buffer, length);
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };
        ctx.on_recv = [this](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            if (respond && responses.size() < requests.size()) {
                // khc has sent the whole request by the time it reads.
                responses.resize(requests.size() - 1);
                responses.push_back(respond(requests.back()));
            }
            const std::string& resp = current();
            size_t n = resp.size() - pos;
            if (n > length_to_read) {
                n = length_to_read;
            }
            memcpy(buffer, resp.data() + pos, n);
            pos += n;
            *out_actual_length = n;
            return KHC_SOCK_OK;
        };
        ctx.on_close = [](void*) { return KHC_SOCK_OK; };
    }

    HttpScript(const HttpScript&) = delete;
    HttpScript& operator=(const HttpScript&) = delete;

    const std::string& current() const {
        static const std::string none;
        size_t i = requests.size() - 1;
        return (i < responses.size()) ? responses[i] : none;
    }

    void attach(khc* http) {
        khc_set_cb_sock_connect(http, khct::cb::mock_connect, &ctx);
        khc_set_cb_sock_send(http, khct::cb::mock_send, &ctx);
        khc_set_cb_sock_recv(http, khct::cb::mock_recv, &ctx);
        khc_set_cb_sock_close(http, khct::cb::mock_close, &ctx);
    }

#ifdef KII_H
    void attach(kii_t* kii) {
        kii_set_cb_http_sock_connect(kii, khct::cb::mock_connect, &ctx);
        kii_set_cb_http_sock_send(kii, khct::cb::mock_send, &ctx);
        kii_set_cb_http_sock_recv(kii, khct::cb::mock_recv, &ctx);
        kii_set_cb_http_sock_close(kii, khct::cb::mock_close, &ctx);
    }
#endif
};

inline bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}

#endif
