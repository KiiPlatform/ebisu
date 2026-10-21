#define CATCH_CONFIG_MAIN
// tio against Kii Cloud, end to end. A user owning a thing sends it commands;
// tio's handler receives them over MQTT, runs their actions through the action
// callback and posts the results, which the user reads back. tio's updater
// uploads the thing's state, which the user reads back too. The two share a
// session, and go on working through a token the server refuses, renewed once
// for both by onboarding again, as are MQTT credentials the broker refuses: tio
// reaches Thing-IF and the broker, and never Kii Cloud's own API.
//
// The test sets the application up itself, as its admin, with CLIENT_ID and
// CLIENT_SECRET from the environment: a trait with an action of each JSON type
// tio hands over, aliased twice on a thing type of this suite's own. The user
// and the thing each run creates are deleted at its end.
//
// Tokens never reach an assertion or a log: CI logs are public. See rest.h.
//
// The handler's MQTT connection goes through the platform sample's socket
// callbacks, whose receives time out as the push task needs; its HTTP requests
// and the updater's go through the large tests' own.

#include "catch.hpp"
#include <kii.h>
#include <tio.h>
#include "rest.h"

#ifdef _WIN32
#include "sock_cb_win32.h"
#else
#include "sock_cb_linux.h"
#include <signal.h>
#endif

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <string.h>

namespace {

const char THING_TYPE[] = "ltest_tio_type";
const char FIRMWARE_VERSION[] = "ltest_tio_fw";
const char TRAIT[] = "ltest_tio_trait";
const char ALIAS1[] = "ltest_tio_alias1";
const char ALIAS2[] = "ltest_tio_alias2";

// Seconds.
const unsigned int TO_RECV = 3;
const unsigned int TO_SEND = 10;
const uint32_t KEEP_ALIVE = 10;
const uint32_t UPDATE_INTERVAL = 3;

// Where the test has got to, for a run that crashes before it can report:
// ctest shows a failed test's output. Step names only, nothing the server
// returned.
void progress(const char* step) {
    printf("tio test: %s\n", step);
    fflush(stdout);
}

std::string app_path(const std::string& rest) {
    return "/api/apps/" + std::string(APP_ID) + rest;
}

std::string thing_if_path(const std::string& rest) {
    return "/thing-if/apps/" + std::string(APP_ID) + rest;
}

// The thing type, its firmware version, the trait and the aliases, unless they
// are there already. Two runs at once, as CI's cells are, may both create the
// trait; the second only adds a version 2, which nothing uses.
void set_up_application(kiiltest::Admin& admin)
{
    const std::string thing_type = app_path("/configuration/thing-types/" + std::string(THING_TYPE));
    kiiltest::Response r = admin.call("PUT", thing_type,
            "application/vnd.kii.ThingTypeConfigurationRequest+json",
            "{\"simpleFlow\":true,\"verificationCodeFlowStartedByUser\":true,"
            "\"verificationCodeFlowStartedByThing\":true,\"verificationCodeTimeout\":172800,"
            "\"verificationCodeLength\":9}");
    INFO( "thing type: " << r.describe() );
    REQUIRE( r.ok() );

    r = admin.call("PUT", thing_type + "/firmware-versions/" + FIRMWARE_VERSION, "text/plain", "");
    INFO( "firmware version: " << r.describe() );
    REQUIRE( (r.ok() || r.status == 409) );

    const std::string trait = thing_if_path("/traits/" + std::string(TRAIT) + "/versions");
    r = admin.call("GET", trait + "/1", "", "");
    if (r.status == 404) {
        auto schema = [](const char* name, const char* type) {
            return "{\"" + std::string(name) + "\":{\"description\":\"\",\"payloadSchema\":{\"type\":\"" +
                type + "\"}}}";
        };
        const std::string definition = "{\"dataGroupingInterval\":\"15_MINUTES\",\"actions\":[" +
            schema("turnPower", "boolean") + "," +
            schema("setPresetTemperature", "integer") + "," +
            schema("setBrightness", "number") + "," +
            schema("setMode", "string") + "," +
            schema("setTimer", "object") + "," +
            schema("setSchedule", "array") + "],\"states\":[" +
            schema("power", "boolean") + "," +
            schema("presetTemperature", "integer") + "," +
            schema("mode", "string") + "]}";
        r = admin.call("POST", trait, "application/vnd.kii.TraitCreationRequest+json", definition);
        INFO( "trait creation: " << r.describe() );
        REQUIRE( r.ok() );
    } else {
        INFO( "trait: " << r.describe() );
        REQUIRE( r.ok() );
    }

    for (const char* alias : { ALIAS1, ALIAS2 }) {
        r = admin.call("PUT", thing_if_path("/configuration/thing-types/" + std::string(THING_TYPE) +
                    "/firmware-versions/" + FIRMWARE_VERSION + "/aliases/" + alias),
                "application/vnd.kii.TraitAliasCreationRequest+json",
                "{\"trait\":\"" + std::string(TRAIT) + "\",\"traitVersion\":1}");
        INFO( "alias " << alias << ": " << r.describe() );
        REQUIRE( (r.ok() || r.status == 409) );
    }
}

// An action as the action callback was given it.
struct Action {
    std::string alias;
    std::string name;
    tio_data_type_t type;
    // As JSON for an object or an array, the characters for a string, and
    // printed for the others.
    std::string value;
};

struct Device;

// A task's HTTP socket, whose sends the device records.
struct RecordedSocket {
    ebisu::ltest::ssl::SSLData* ssl;
    Device* d;
};

struct Device {
    tio_handler_t handler;
    tio_updater_t updater;
    char handler_http_buff[4096];
    char handler_mqtt_buff[4096];
    char updater_buff[4096];
    jkii_token_t handler_tokens[256];
    jkii_resource_t handler_resource;
    jkii_token_t updater_tokens[256];
    jkii_resource_t updater_resource;
    ebisu::ltest::ssl::SSLData handler_http;
    ebisu::ltest::ssl::SSLData updater_http;
    RecordedSocket handler_sent;
    RecordedSocket updater_sent;
    // What the tasks sent over HTTP, to tell where they went. It holds tokens
    // and the password, so it is searched and never printed.
    std::mutex sent_mutex;
    std::string sent;
    socket_context_t mqtt;
    std::vector<std::thread> tasks;

    kii_session_t session;
    std::mutex session_mutex;
    // How many times the session's tokens changed. Changed with the session's
    // lock held.
    int author_updates = 0;

    std::mutex mutex;
    std::condition_variable changed;
    // Guarded by mutex.
    std::vector<kii_mqtt_task_state> handler_states;
    std::vector<Action> actions;
    std::vector<std::string> errors;
    // What the updater uploads, and what its current upload reads from.
    std::string state;
    std::string upload;
    size_t upload_read = 0;
    int uploads = 0;
    bool stop = false;
    int exits = 0;
};

khc_sock_code_t cb_record_send(void* userdata, const char* buffer, size_t length,
        size_t* out_sent_length)
{
    RecordedSocket* r = (RecordedSocket*)userdata;
    khc_sock_code_t res = ebisu::ltest::ssl::cb_send(r->ssl, buffer, length, out_sent_length);
    if (res == KHC_SOCK_OK) {
        std::lock_guard<std::mutex> lock(r->d->sent_mutex);
        r->d->sent.append(buffer, *out_sent_length);
    }
    return res;
}

bool sent_has(Device& d, const char* text)
{
    std::lock_guard<std::mutex> lock(d.sent_mutex);
    return d.sent.find(text) != std::string::npos;
}

void forget_sent(Device& d)
{
    std::lock_guard<std::mutex> lock(d.sent_mutex);
    d.sent.clear();
}

kii_task_code_t cb_task_create(const char* name, KII_TASK_ENTRY entry,
        void* entry_param, void* userdata)
{
    Device* d = (Device*)userdata;
    d->tasks.emplace_back(entry, entry_param);
    return KII_TASKC_OK;
}

void cb_delay_ms(uint32_t msec, void* userdata)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}

kii_bool_t cb_handler_continue(void* task_info, void* userdata)
{
    Device* d = (Device*)userdata;
    kii_mqtt_task_state state = ((tio_handler_task_info_t*)task_info)->task_state;
    std::lock_guard<std::mutex> lock(d->mutex);
    if (d->handler_states.empty() || d->handler_states.back() != state) {
        d->handler_states.push_back(state);
        d->changed.notify_all();
    }
    return d->stop ? KII_FALSE : KII_TRUE;
}

kii_bool_t cb_updater_continue(void* task_info, void* userdata)
{
    Device* d = (Device*)userdata;
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->stop ? KII_FALSE : KII_TRUE;
}

void cb_exit(void* task_info, void* userdata)
{
    Device* d = (Device*)userdata;
    std::lock_guard<std::mutex> lock(d->mutex);
    ++d->exits;
    d->changed.notify_all();
}

void cb_err(tio_code_t code, const char* err_message, void* userdata)
{
    Device* d = (Device*)userdata;
    std::lock_guard<std::mutex> lock(d->mutex);
    d->errors.push_back(std::string(err_message) + " (" + std::to_string(code) + ")");
}

std::string value_of(const tio_action_value_t& v)
{
    switch (v.type) {
        case TIO_TYPE_BOOLEAN:
            return v.param.bool_value == KII_TRUE ? "true" : "false";
        case TIO_TYPE_INTEGER:
            return std::to_string(v.param.long_value);
        case TIO_TYPE_DOUBLE:
            return std::to_string(v.param.double_value);
        case TIO_TYPE_STRING:
        case TIO_TYPE_OBJECT:
        case TIO_TYPE_ARRAY:
            return std::string(v.param.opaque_value, v.opaque_value_length);
        default:
            return "null";
    }
}

// Records the action. setMode "unsupported" fails, and setPresetTemperature
// reports the temperature applied as its result's data.
tio_bool_t cb_action(tio_action_t* action, tio_action_err_t* err,
        tio_action_result_data_t* data, void* userdata)
{
    Device* d = (Device*)userdata;
    Action a;
    a.alias = std::string(action->alias, action->alias_length);
    a.name = std::string(action->action_name, action->action_name_length);
    a.type = action->action_value.type;
    a.value = value_of(action->action_value);
    {
        std::lock_guard<std::mutex> lock(d->mutex);
        d->actions.push_back(a);
        d->changed.notify_all();
    }
    if (a.name == "setMode" && a.value == "unsupported") {
        snprintf(err->err_message, sizeof(err->err_message), "mode not supported");
        return KII_FALSE;
    }
    if (a.name == "setPresetTemperature") {
        snprintf(data->json, sizeof(data->json), "{\"applied\":%s}", a.value.c_str());
    }
    return KII_TRUE;
}

size_t cb_state_size(void* userdata)
{
    Device* d = (Device*)userdata;
    std::lock_guard<std::mutex> lock(d->mutex);
    d->upload = d->state;
    d->upload_read = 0;
    ++d->uploads;
    return d->upload.size();
}

size_t cb_state_read(char* buffer, size_t size, void* userdata)
{
    Device* d = (Device*)userdata;
    std::lock_guard<std::mutex> lock(d->mutex);
    size_t n = d->upload.copy(buffer, size, d->upload_read);
    d->upload_read += n;
    return n;
}

void cb_session_lock(void* userdata)
{
    ((std::mutex*)userdata)->lock();
}

void cb_session_unlock(void* userdata)
{
    ((std::mutex*)userdata)->unlock();
}

uint64_t cb_session_now_ms(void* userdata)
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
}

void cb_author_updated(const kii_author_t* author, void* userdata)
{
    Device* d = (Device*)userdata;
    ++d->author_updates;
}

int author_updates(Device& d)
{
    std::lock_guard<std::mutex> lock(d.session_mutex);
    return d.author_updates;
}

// A token the server doesn't know, which it treats as it does an expired one.
const char UNKNOWN_TOKEN[] = "ltest-unknown-token";

// Has the session hold its author with an access token the server refuses.
void spoil(kii_session_t* session)
{
    kii_author_t author;
    kii_session_get_author(session, &author);
    strcpy(author.access_token, UNKNOWN_TOKEN);
    kii_session_set_author(session, &author);
}

// Whether the session still holds the token spoil() gave it. Compared here
// rather than in an assertion, which would print the token, into logs that are
// public.
bool spoiled(kii_session_t* session)
{
    kii_author_t author;
    kii_session_get_author(session, &author);
    return strcmp(author.access_token, UNKNOWN_TOKEN) == 0;
}

void init_device(Device& d)
{
    d.handler_resource = {d.handler_tokens, sizeof(d.handler_tokens) / sizeof(d.handler_tokens[0])};
    d.updater_resource = {d.updater_tokens, sizeof(d.updater_tokens) / sizeof(d.updater_tokens[0])};
#ifdef _WIN32
    sock_cb_init_context(&d.mqtt, TO_RECV, TO_SEND);
#else
    memset(&d.mqtt, 0, sizeof(d.mqtt));
    d.mqtt.to_recv = TO_RECV;
    d.mqtt.to_send = TO_SEND;
#endif

    tio_handler_t* h = &d.handler;
    tio_handler_init(h);
    tio_handler_set_app(h, APP_ID, DEFAULT_SITE);
    tio_handler_set_cb_task_create(h, cb_task_create, &d);
    tio_handler_set_cb_delay_ms(h, cb_delay_ms, NULL);
    tio_handler_set_cb_task_continue(h, cb_handler_continue, &d);
    tio_handler_set_cb_task_exit(h, cb_exit, &d);
    tio_handler_set_cb_err(h, cb_err, &d);
    tio_handler_set_cb_sock_connect_http(h, ebisu::ltest::ssl::cb_connect, &d.handler_http);
    d.handler_sent = {&d.handler_http, &d};
    tio_handler_set_cb_sock_send_http(h, cb_record_send, &d.handler_sent);
    tio_handler_set_cb_sock_recv_http(h, ebisu::ltest::ssl::cb_recv, &d.handler_http);
    tio_handler_set_cb_sock_close_http(h, ebisu::ltest::ssl::cb_close, &d.handler_http);
    tio_handler_set_cb_sock_connect_mqtt(h, sock_cb_connect, &d.mqtt);
    tio_handler_set_cb_sock_send_mqtt(h, sock_cb_send, &d.mqtt);
    tio_handler_set_cb_sock_recv_mqtt(h, sock_cb_recv, &d.mqtt);
    tio_handler_set_cb_sock_recv_timed_out_mqtt(h, sock_cb_recv_timed_out, &d.mqtt);
    tio_handler_set_cb_sock_close_mqtt(h, sock_cb_close, &d.mqtt);
    tio_handler_set_mqtt_to_sock_recv(h, TO_RECV);
    tio_handler_set_mqtt_to_sock_send(h, TO_SEND);
    tio_handler_set_http_buff(h, d.handler_http_buff, sizeof(d.handler_http_buff));
    tio_handler_set_mqtt_buff(h, d.handler_mqtt_buff, sizeof(d.handler_mqtt_buff));
    tio_handler_set_keep_alive_interval(h, KEEP_ALIVE);
    tio_handler_set_json_parser_resource(h, &d.handler_resource);

    tio_updater_t* u = &d.updater;
    tio_updater_init(u);
    tio_updater_set_app(u, APP_ID, DEFAULT_SITE);
    tio_updater_set_cb_task_create(u, cb_task_create, &d);
    tio_updater_set_cb_delay_ms(u, cb_delay_ms, NULL);
    tio_updater_set_cb_task_continue(u, cb_updater_continue, &d);
    tio_updater_set_cb_task_exit(u, cb_exit, &d);
    tio_updater_set_cb_error(u, cb_err, &d);
    tio_updater_set_cb_sock_connect(u, ebisu::ltest::ssl::cb_connect, &d.updater_http);
    d.updater_sent = {&d.updater_http, &d};
    tio_updater_set_cb_sock_send(u, cb_record_send, &d.updater_sent);
    tio_updater_set_cb_sock_recv(u, ebisu::ltest::ssl::cb_recv, &d.updater_http);
    tio_updater_set_cb_sock_close(u, ebisu::ltest::ssl::cb_close, &d.updater_http);
    tio_updater_set_buff(u, d.updater_buff, sizeof(d.updater_buff));
    tio_updater_set_interval(u, UPDATE_INTERVAL);
    tio_updater_set_json_parser_resource(u, &d.updater_resource);

    kii_session_init(&d.session);
    kii_session_set_cb_lock(&d.session, cb_session_lock, cb_session_unlock, &d.session_mutex);
    kii_session_set_cb_monotonic_ms(&d.session, cb_session_now_ms, NULL);
    kii_session_set_cb_author_updated(&d.session, cb_author_updated, &d);
}

// Waits up to seconds for done(), called with the device's mutex held.
bool wait_for(Device& d, int seconds, std::function<bool()> done)
{
    std::unique_lock<std::mutex> lock(d.mutex);
    return d.changed.wait_for(lock, std::chrono::seconds(seconds), done);
}

// What the tasks reported through the error callback so far: tio's fixed
// messages and codes.
std::string errors_of(Device& d)
{
    std::lock_guard<std::mutex> lock(d.mutex);
    std::string errors;
    for (const std::string& e : d.errors) {
        errors += (errors.empty() ? "" : "; ") + e;
    }
    return errors.empty() ? "none" : errors;
}

// Polls fetch() once a second, up to seconds, until done() holds for what it
// returned.
kiiltest::Response poll(int seconds, std::function<kiiltest::Response()> fetch,
        std::function<bool(const kiiltest::Response&)> done)
{
    kiiltest::Response r;
    for (int i = 0; i < seconds; ++i) {
        r = fetch();
        if (done(r)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return r;
}

// Stops the tasks and waits for them to end, whichever way the test leaves,
// so that they never outlive what they use.
struct StopDevice {
    Device& d;
    ~StopDevice() {
        {
            std::lock_guard<std::mutex> lock(d.mutex);
            d.stop = true;
        }
        for (std::thread& t : d.tasks) {
            if (t.joinable()) {
                t.join();
            }
        }
        progress("tasks joined");
    }
};

const picojson::value* member(const picojson::value& v, const std::string& name)
{
    if (!v.is<picojson::object>()) {
        return NULL;
    }
    const picojson::object& o = v.get<picojson::object>();
    auto it = o.find(name);
    return it == o.end() ? NULL : &it->second;
}

// The result reported for alias's action name, from a command's
// actionResults: [{alias:[{name:{succeeded:...}}, ...]}, ...].
picojson::value result_of(const kiiltest::Response& command, const std::string& alias,
        const std::string& name)
{
    const picojson::value* results = member(command.body, "actionResults");
    if (results == NULL || !results->is<picojson::array>()) {
        return picojson::value();
    }
    for (const picojson::value& by_alias : results->get<picojson::array>()) {
        const picojson::value* actions = member(by_alias, alias);
        if (actions == NULL || !actions->is<picojson::array>()) {
            continue;
        }
        for (const picojson::value& action : actions->get<picojson::array>()) {
            const picojson::value* result = member(action, name);
            if (result != NULL) {
                return *result;
            }
        }
    }
    return picojson::value();
}

}

TEST_CASE("tio against Kii Cloud")
{
    if (!kiiltest::Admin::configured()) {
        // Never in CI, which exports them: skipping there would hide the suite.
        if (getenv("CI") != NULL) {
            FAIL( "CLIENT_ID or CLIENT_SECRET is not set, or empty" );
        }
        WARN( "CLIENT_ID or CLIENT_SECRET is not set, or empty: the tio suite is skipped" );
        return;
    }
#ifndef _WIN32
    // A write to a connection the peer has reset fails with EPIPE rather than
    // ending the process, as in the samples.
    signal(SIGPIPE, SIG_IGN);
#endif

    kiiltest::Rest rest;
    kiiltest::Admin admin(rest);
    set_up_application(admin);
    progress("application set up");

    // A user and a thing of the run's own, so that no other run, such as the
    // other CI cell's, sees what this one sends.
    const std::string run = std::to_string(kiiltest::current_time());
    kiiltest::User user(rest, admin, "ltest_tio_" + run);

    // The user onboards the thing, as an application does, and so owns it.
    const std::string vendor_thing_id = "ltest_tio_" + run;
    const std::string thing_password = kiiltest::random_password();
    picojson::object onboarding;
    onboarding["vendorThingID"] = picojson::value(vendor_thing_id);
    onboarding["thingPassword"] = picojson::value(thing_password);
    onboarding["owner"] = picojson::value("user:" + user.id());
    onboarding["thingType"] = picojson::value(std::string(THING_TYPE));
    onboarding["firmwareVersion"] = picojson::value(std::string(FIRMWARE_VERSION));
    kiiltest::Response onboarded = user.call("POST", thing_if_path("/onboardings"),
            "application/vnd.kii.OnboardingWithVendorThingIDByOwner+json", kiiltest::json(onboarding));
    INFO( "onboarding by the owner: " << onboarded.describe() );
    REQUIRE( onboarded.ok() );
    const std::string thing_id = onboarded.get("thingID");
    REQUIRE_FALSE( thing_id.empty() );
    admin.delete_at_end(app_path("/things/" + thing_id));
    progress("thing onboarded by its owner");
    const std::string target = thing_if_path("/targets/thing:" + thing_id);

    // The device onboards with the same vendor thing ID and password, into the
    // session its tasks share, and starts them with the session's author.
    Device d;
    init_device(d);
    kii_session_set_password(&d.session, vendor_thing_id.c_str(), thing_password.c_str());
    REQUIRE( tio_handler_set_session(&d.handler, &d.session) == TIO_ERR_OK );
    REQUIRE( tio_updater_set_session(&d.updater, &d.session) == TIO_ERR_OK );
    d.state = "{\"" + std::string(ALIAS1) + "\":{\"power\":true,\"presetTemperature\":23},\"" +
        ALIAS2 + "\":{\"mode\":\"first-" + run + "\"}}";
    REQUIRE( tio_handler_onboard(&d.handler, vendor_thing_id.c_str(), thing_password.c_str(),
                THING_TYPE, FIRMWARE_VERSION, NULL, NULL) == TIO_ERR_OK );
    {
        kii_author_t author;
        kii_session_get_author(&d.session, &author);
        REQUIRE( std::string(author.author_id) == thing_id );
    }
    CHECK( author_updates(d) == 1 );
    progress("device onboarded");
    // The broker refuses the MQTT credentials onboarding returned, as it would
    // once they were revoked.
    strcpy(d.handler._kii._onboarded_endpoint.password, "ltest-refused");
    forget_sent(d);
    StopDevice stop_device{d};
    REQUIRE( tio_handler_start(&d.handler, NULL, cb_action, &d) == TIO_ERR_OK );
    REQUIRE( tio_updater_start(&d.updater, NULL, cb_state_size, &d, cb_state_read, &d) == TIO_ERR_OK );
    progress("tasks started");

    // The handler connects with the MQTT endpoint onboarding returned, without
    // installing push or fetching the endpoint. Refused, it gets new
    // credentials by onboarding again, which renews the token too, and
    // subscribes.
    REQUIRE( wait_for(d, 120, [&d] {
        return std::find(d.handler_states.begin(), d.handler_states.end(),
                KII_MQTT_ST_RECV_READY) != d.handler_states.end() || d.exits > 0;
    }) );
    {
        std::lock_guard<std::mutex> lock(d.mutex);
        REQUIRE( d.exits == 0 );
        CHECK( std::count(d.handler_states.begin(), d.handler_states.end(), KII_MQTT_ST_INSTALL_PUSH) == 0 );
        CHECK( std::count(d.handler_states.begin(), d.handler_states.end(), KII_MQTT_ST_GET_ENDPOINT) == 1 );
    }
    CHECK( sent_has(d, "/onboardings HTTP/1.1") );
    CHECK_FALSE( sent_has(d, "/installations") );
    CHECK( author_updates(d) == 2 );
    progress("handler ready");

    auto send_command = [&user, &target](const std::string& actions) {
        kiiltest::Response sent = user.call("POST", target + "/commands",
                "application/vnd.kii.CommandCreationRequest+json",
                "{\"issuer\":\"user:" + user.id() + "\",\"actions\":" + actions + "}");
        INFO( "command: " << sent.describe() );
        REQUIRE( sent.ok() );
        std::string command_id = sent.get("commandID");
        REQUIRE_FALSE( command_id.empty() );
        return command_id;
    };
    // The command once its results are in.
    auto finished = [&user, &target](const std::string& command_id) {
        return poll(60, [&] {
            return user.call("GET", target + "/commands/" + command_id, "", "");
        }, [](const kiiltest::Response& r) {
            std::string state = r.get("commandState");
            return state == "DONE" || state == "INCOMPLETE";
        });
    };

    // A command's actions, of every type, run in order, and their results
    // reach the user. One step after another rather than in sections, which
    // Catch would each run from the top: a user, a thing and a connection each.
    {
        const std::string command_id = send_command(std::string("[") +
                "{\"" + ALIAS1 + "\":[{\"turnPower\":true},{\"setPresetTemperature\":23}]}," +
                "{\"" + ALIAS2 + "\":[{\"setBrightness\":0.5},{\"setMode\":\"eco\"}," +
                "{\"setTimer\":{\"minutes\":30,\"off\":true}},{\"setSchedule\":[8,20]}]}]");

        REQUIRE( wait_for(d, 60, [&d] { return d.actions.size() >= 6; }) );
        std::vector<Action> actions;
        {
            std::lock_guard<std::mutex> lock(d.mutex);
            actions = d.actions;
        }
        REQUIRE( actions.size() == 6 );
        // In the command's order.
        const struct {
            const char* alias;
            const char* name;
            tio_data_type_t type;
            const char* value;
        } expected[] = {
            { ALIAS1, "turnPower", TIO_TYPE_BOOLEAN, "true" },
            { ALIAS1, "setPresetTemperature", TIO_TYPE_INTEGER, "23" },
            { ALIAS2, "setBrightness", TIO_TYPE_DOUBLE, "0.500000" },
            { ALIAS2, "setMode", TIO_TYPE_STRING, "eco" },
            { ALIAS2, "setTimer", TIO_TYPE_OBJECT, NULL },
            { ALIAS2, "setSchedule", TIO_TYPE_ARRAY, NULL },
        };
        for (size_t i = 0; i < actions.size(); ++i) {
            INFO( "action " << i );
            CHECK( actions[i].alias == expected[i].alias );
            CHECK( actions[i].name == expected[i].name );
            CHECK( actions[i].type == expected[i].type );
            if (expected[i].value != NULL) {
                CHECK( actions[i].value == expected[i].value );
            }
        }
        // An object and an array arrive as their JSON, as the server sent it.
        picojson::value timer;
        REQUIRE( picojson::parse(timer, actions[4].value).empty() );
        CHECK( timer.serialize() == "{\"minutes\":30,\"off\":true}" );
        picojson::value schedule;
        REQUIRE( picojson::parse(schedule, actions[5].value).empty() );
        CHECK( schedule.serialize() == "[8,20]" );

        kiiltest::Response command = finished(command_id);
        INFO( "command results: " << command.describe() );
        INFO( "errors reported by the device: " << errors_of(d) );
        REQUIRE( command.ok() );
        CHECK( command.get("commandState") == "DONE" );
        for (const Action& a : actions) {
            INFO( a.alias << " " << a.name );
            picojson::value result = result_of(command, a.alias, a.name);
            const picojson::value* succeeded = member(result, "succeeded");
            REQUIRE( succeeded != nullptr );
            CHECK( succeeded->is<bool>() );
            CHECK( succeeded->evaluate_as_boolean() );
        }
        // The data the action callback set.
        // Kept: member() points into it.
        const picojson::value temperature = result_of(command, ALIAS1, "setPresetTemperature");
        const picojson::value* data = member(temperature, "data");
        REQUIRE( data != nullptr );
        CHECK( data->serialize() == "{\"applied\":23}" );
    }
    progress("first command done");

    // An action that fails leaves the command incomplete, with its error.
    {
        std::lock_guard<std::mutex> lock(d.mutex);
        d.actions.clear();
    }
    {
        const std::string command_id = send_command(std::string("[") +
                "{\"" + ALIAS1 + "\":[{\"turnPower\":false}]}," +
                "{\"" + ALIAS2 + "\":[{\"setMode\":\"unsupported\"}]}]");

        kiiltest::Response command = finished(command_id);
        INFO( "command results: " << command.describe() );
        INFO( "errors reported by the device: " << errors_of(d) );
        REQUIRE( command.ok() );
        CHECK( command.get("commandState") == "INCOMPLETE" );
        const picojson::value power_result = result_of(command, ALIAS1, "turnPower");
        const picojson::value* power = member(power_result, "succeeded");
        REQUIRE( power != nullptr );
        CHECK( power->evaluate_as_boolean() );
        picojson::value mode = result_of(command, ALIAS2, "setMode");
        const picojson::value* succeeded = member(mode, "succeeded");
        REQUIRE( succeeded != nullptr );
        CHECK_FALSE( succeeded->evaluate_as_boolean() );
        const picojson::value* message = member(mode, "errorMessage");
        REQUIRE( message != nullptr );
        CHECK( message->serialize() == "\"mode not supported\"" );
    }
    progress("second command done");

    // Reads the state until its mode is expected, up to a minute.
    auto read_mode = [&user, &target](const std::string& expected) {
        kiiltest::Response r = poll(60, [&] {
            return user.call("GET", target + "/states", "", "");
        }, [&expected](const kiiltest::Response& r) {
            const picojson::value* alias = member(r.body, ALIAS2);
            const picojson::value* mode = alias == NULL ? NULL : member(*alias, "mode");
            return mode != NULL && mode->is<std::string>() && mode->get<std::string>() == expected;
        });
        INFO( "state: " << r.describe() );
        REQUIRE( r.ok() );
        return r;
    };

    // The updater's uploads reach the user, the latest each time.
    {
        kiiltest::Response first = read_mode("first-" + run);
        const picojson::value* alias1 = member(first.body, ALIAS1);
        REQUIRE( alias1 != nullptr );
        CHECK( alias1->serialize() == "{\"power\":true,\"presetTemperature\":23}" );

        {
            std::lock_guard<std::mutex> lock(d.mutex);
            d.state = "{\"" + std::string(ALIAS1) + "\":{\"power\":false,\"presetTemperature\":20},\"" +
                ALIAS2 + "\":{\"mode\":\"next-" + run + "\"}}";
        }
        kiiltest::Response next = read_mode("next-" + run);
        alias1 = member(next.body, ALIAS1);
        REQUIRE( alias1 != nullptr );
        CHECK( alias1->serialize() == "{\"power\":false,\"presetTemperature\":20}" );
    }
    progress("states read back");

    // The server refuses the tasks' token. Whichever task is refused first has
    // the session renew it, once, and the other takes the new one: the updater
    // uploads again, from the start of the state, and a command's results are
    // sent again, its action run once.
    CHECK( author_updates(d) == 2 );
    spoil(&d.session);
    {
        std::lock_guard<std::mutex> lock(d.mutex);
        d.actions.clear();
        d.state = "{\"" + std::string(ALIAS1) + "\":{\"power\":true,\"presetTemperature\":21},\"" +
            ALIAS2 + "\":{\"mode\":\"renewed-" + run + "\"}}";
    }
    {
        const std::string command_id = send_command(std::string("[") +
                "{\"" + ALIAS1 + "\":[{\"turnPower\":true}]}]");
        kiiltest::Response command = finished(command_id);
        INFO( "command results: " << command.describe() );
        INFO( "errors reported by the device: " << errors_of(d) );
        REQUIRE( command.ok() );
        CHECK( command.get("commandState") == "DONE" );
        std::lock_guard<std::mutex> lock(d.mutex);
        CHECK( d.actions.size() == 1 );
    }
    {
        kiiltest::Response renewed = read_mode("renewed-" + run);
        const picojson::value* alias1 = member(renewed.body, ALIAS1);
        REQUIRE( alias1 != nullptr );
        CHECK( alias1->serialize() == "{\"power\":true,\"presetTemperature\":21}" );
    }
    CHECK_FALSE( spoiled(&d.session) );
    CHECK( author_updates(d) == 3 );
    // Since the tasks started, nothing went to Kii Cloud's own API: neither
    // renewal asked its token endpoint for anything.
    CHECK_FALSE( sent_has(d, "/api/apps/") );
    progress("token renewed");

    // Asked to stop, both tasks end: the handler within a receive timeout, the
    // updater within a second of its interval.
    {
        std::lock_guard<std::mutex> lock(d.mutex);
        d.stop = true;
    }
    CHECK( wait_for(d, TO_RECV * 3 + UPDATE_INTERVAL + 10, [&d] { return d.exits == 2; }) );
    progress("tasks ended");
    {
        std::lock_guard<std::mutex> lock(d.mutex);
        for (const std::string& e : d.errors) {
            UNSCOPED_INFO( e );
        }
        CHECK( d.errors.empty() );
    }
}
