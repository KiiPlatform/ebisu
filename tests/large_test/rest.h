#ifndef __kiiltest_rest__
#define __kiiltest_rest__

// Requests a test makes to Kii Cloud as someone other than the thing it tests:
// the application's admin setting the application up and cleaning up after
// the test, or a user owning the thing. Each goes through a kii_t of its own,
// with kii_api_call_*().
//
// The repository's CI logs are public, so tokens never reach an assertion, an
// INFO or a print: Catch prints both sides of a failed assertion, and a
// response body may hold a token. A failed request is described by its status
// and errorCode only. The admin and user tokens are requested to expire within
// minutes, and the admin's is revoked at the end.

#include <chrono>
#include <cstdlib>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <picojson.h>
#include <catch.hpp>
#include <kii.h>
#include "kii/large_test.h"

namespace kiiltest {

// How long the admin and user tokens last: a run takes a few minutes, longer
// under valgrind.
const int64_t TOKEN_LIFETIME_MS = 20 * 60 * 1000;

inline int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}

// A password for something a run creates.
inline std::string random_password() {
    std::random_device rd;
    std::string password;
    const char digits[] = "0123456789abcdef";
    for (int i = 0; i < 24; ++i) {
        password += digits[rd() % 16];
    }
    return password;
}

inline std::string json(const picojson::object& o) {
    return picojson::value(o).serialize();
}

struct Response {
    kii_code_t res = KII_ERR_FAIL;
    int status = 0;
    // Null unless the body is JSON.
    picojson::value body;

    bool ok() const {
        return res == KII_ERR_OK && 200 <= status && status < 300;
    }

    // A top-level string field of the body, or "".
    std::string get(const std::string& field) const {
        if (!body.is<picojson::object>()) {
            return "";
        }
        const picojson::object& o = body.get<picojson::object>();
        auto it = o.find(field);
        if (it == o.end() || !it->second.is<std::string>()) {
            return "";
        }
        return it->second.get<std::string>();
    }

    // What went wrong, without the body, which may hold a token: the
    // errorCode, and for the token endpoint's errors their description, which
    // is one of its fixed messages.
    std::string describe() const {
        std::string d = "kii_code_t " + std::to_string(res) + ", status " + std::to_string(status);
        std::string error_code = get("errorCode");
        if (!error_code.empty()) {
            d += ", errorCode " + error_code;
        }
        std::string description = get("error_description");
        if (!description.empty()) {
            d += ": " + description;
        }
        return d;
    }
};

class Rest {
public:
    Rest() {
        resource_ = {tokens_, sizeof(tokens_) / sizeof(tokens_[0])};
        init(&kii_, buff_, sizeof(buff_), &http_ctx_, &mqtt_ctx_, &resource_);
    }

    Rest(const Rest&) = delete;
    Rest& operator=(const Rest&) = delete;

    // token, when not empty, authorizes the request. A 429 is retried.
    Response call(const std::string& method, const std::string& path,
            const std::string& content_type, const std::string& body,
            const std::string& token) {
        Response r;
        for (int attempt = 0; attempt < 5; ++attempt) {
            if (attempt > 0) {
                std::this_thread::sleep_for(std::chrono::seconds(attempt));
            }
            r = send(method, path, content_type, body, token);
            if (r.res != KII_ERR_OK || r.status != 429) {
                break;
            }
        }
        return r;
    }

private:
    Response send(const std::string& method, const std::string& path,
            const std::string& content_type, const std::string& body,
            const std::string& token) {
        Response r;
        r.res = kii_api_call_start(&kii_, method.c_str(), path.c_str(),
                content_type.empty() ? NULL : content_type.c_str(), KII_FALSE);
        if (r.res == KII_ERR_OK && !token.empty()) {
            r.res = kii_api_call_append_header(&kii_, "Authorization", ("Bearer " + token).c_str());
        }
        if (r.res == KII_ERR_OK && !body.empty()) {
            r.res = kii_api_call_append_body(&kii_, body.data(), body.size());
        }
        if (r.res == KII_ERR_OK) {
            r.res = kii_api_call_run(&kii_);
        }
        if (r.res != KII_ERR_OK) {
            return r;
        }
        r.status = kii_get_resp_status(&kii_);
        size_t length = kii_get_resp_body_length(&kii_);
        picojson::value v;
        if (length > 0 && picojson::parse(v, std::string(buff_, length)).empty()) {
            r.body = v;
        }
        return r;
    }

    kii_t kii_;
    char buff_[16384];
    jkii_token_t tokens_[64];
    jkii_resource_t resource_;
    ebisu::ltest::ssl::SSLData http_ctx_;
    ebisu::ltest::ssl::SSLData mqtt_ctx_;
};

// The application's admin, from CLIENT_ID and CLIENT_SECRET in the
// environment. What is registered with delete_at_end() is deleted when the
// admin goes out of scope, latest first, and then its token is revoked.
class Admin {
public:
    // Whether the environment has the admin's credentials. CI exports them;
    // an undefined secret is exported empty.
    static bool configured() {
        const char* id = getenv("CLIENT_ID");
        const char* secret = getenv("CLIENT_SECRET");
        return id != NULL && id[0] != '\0' && secret != NULL && secret[0] != '\0';
    }

    explicit Admin(Rest& rest) : rest_(rest) {
        picojson::object request;
        request["client_id"] = picojson::value(std::string(getenv("CLIENT_ID")));
        request["client_secret"] = picojson::value(std::string(getenv("CLIENT_SECRET")));
        request["expiresAt"] = picojson::value((double)(now_ms() + TOKEN_LIFETIME_MS));
        Response r = rest_.call("POST", "/api/oauth2/token", "application/json", json(request), "");
        INFO( "admin token: " << r.describe() );
        REQUIRE( r.ok() );
        token_ = r.get("access_token");
        REQUIRE_FALSE( token_.empty() );
    }

    ~Admin() {
        for (auto it = to_delete_.rbegin(); it != to_delete_.rend(); ++it) {
            call("DELETE", *it, "", "");
        }
        picojson::object request;
        request["type"] = picojson::value(std::string("SPECIFIC_TOKEN"));
        request["token"] = picojson::value(token_);
        call("POST", "/api/apps/" + std::string(APP_ID) + "/logout",
                "application/vnd.kii.LogoutRequest+json", json(request));
    }

    Admin(const Admin&) = delete;
    Admin& operator=(const Admin&) = delete;

    Response call(const std::string& method, const std::string& path,
            const std::string& content_type, const std::string& body) {
        return rest_.call(method, path, content_type, body, token_);
    }

    void delete_at_end(const std::string& path) {
        to_delete_.push_back(path);
    }

private:
    Rest& rest_;
    std::string token_;
    std::vector<std::string> to_delete_;
};

// A user of the run's own, created by the admin and deleted with it, with a
// short-lived token.
class User {
public:
    User(Rest& rest, Admin& admin, const std::string& login_name) : rest_(rest) {
        const std::string password = random_password();
        picojson::object registration;
        registration["loginName"] = picojson::value(login_name);
        registration["password"] = picojson::value(password);
        Response created = admin.call("POST", "/api/apps/" + std::string(APP_ID) + "/users",
                "application/vnd.kii.RegistrationRequest+json", json(registration));
        INFO( "user registration: " << created.describe() );
        REQUIRE( created.ok() );
        id_ = created.get("userID");
        REQUIRE_FALSE( id_.empty() );
        admin.delete_at_end("/api/apps/" + std::string(APP_ID) + "/users/" + id_);

        picojson::object request;
        request["grant_type"] = picojson::value(std::string("password"));
        request["username"] = picojson::value(login_name);
        request["password"] = picojson::value(password);
        request["expiresAt"] = picojson::value((double)(now_ms() + TOKEN_LIFETIME_MS));
        Response r = rest_.call("POST", "/api/apps/" + std::string(APP_ID) + "/oauth2/token",
                "application/json", json(request), "");
        INFO( "user token: " << r.describe() );
        REQUIRE( r.ok() );
        token_ = r.get("access_token");
        REQUIRE_FALSE( token_.empty() );
    }

    User(const User&) = delete;
    User& operator=(const User&) = delete;

    const std::string& id() const {
        return id_;
    }

    Response call(const std::string& method, const std::string& path,
            const std::string& content_type, const std::string& body) {
        return rest_.call(method, path, content_type, body, token_);
    }

private:
    Rest& rest_;
    std::string id_;
    std::string token_;
};

}

#endif
