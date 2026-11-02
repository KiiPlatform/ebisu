#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "kii_mqtt_task.h"
#include "kii.h"
#include "kii_session_impl.h"

static void _delay_ms(kii_t* kii, uint32_t msec)
{
    kii->_system->ops->delay_ms(kii->_system, msec);
}

#define WAIT_MS 1000
#define ARRIVED_MSG_READ_TIME 500
#define MSG_SEND_TIME 500
/* The wait before another attempt to reach the cloud doubles from the first of
 * these to the second, unless kii_set_mqtt_retry_wait_max() set another, and
 * what the task waits is half to all of that. */
#define RETRY_WAIT_MIN_MS 2000
#define RETRY_WAIT_MAX_MS 300000
/* A broker that cannot be reached after this many attempts may have moved, so
 * the task asks for the endpoint again. */
#define MAX_CONNECT_FAILURES 8

/* What CONNECT names the version of MQTT it speaks with: the protocol name,
 * and the protocol level. */
static const char* _mqtt_protocol_name(const kii_t* kii)
{
    return kii->_mqtt_version == KII_MQTT_VERSION_3_1 ? "MQIsdp" : "MQTT";
}

static char _mqtt_protocol_level(const kii_t* kii)
{
    return kii->_mqtt_version == KII_MQTT_VERSION_3_1 ? 0x03 : 0x04;
}

int _mqtt_encode(char* buf, unsigned long remaining_length)
{
    int rc = 0;
    char d;

    do
    {
        d = remaining_length % 128;
        remaining_length /= 128;
        if(remaining_length > 0)
            d |= 0x80;
        buf[rc++] = d;
    }
    while(remaining_length > 0 && rc < 5);
    return rc;
}

int _mqtt_decode(char* buf, unsigned long* remaining_length)
{
    int i = 0;
    int multiplier = 1;
    *remaining_length = 0;
    do
    {
        if(i > 3)
        {
            return -1;
        }
        *remaining_length += (buf[i] & 127) * multiplier;
        multiplier *= 128;
    }
    while((buf[i++] & 128) != 0);

    return i;
}

ebisu_sock_code_t _mqtt_send_connect(kii_t* kii, kii_mqtt_endpoint_t* endpoint) {
    uint16_t keep_alive_interval = kii->_keep_alive_interval;
    memset(kii->_mqtt_buffer, 0, kii->_mqtt_buffer_size);
    int i = 8; /* reserver 8 bytes for header */
    /* Variable header:Protocol Name bytes */
    const char* protocol_name = _mqtt_protocol_name(kii);
    const size_t protocol_name_len = strlen(protocol_name);
    kii->_mqtt_buffer[i++] = 0x00;
    kii->_mqtt_buffer[i++] = (char)protocol_name_len;
    memcpy(&kii->_mqtt_buffer[i], protocol_name, protocol_name_len);
    i += protocol_name_len;
    /* Variable header:Protocol Level */
    kii->_mqtt_buffer[i++] = _mqtt_protocol_level(kii);
    /* Variable header:Connect Flags */
    /*
    * Bit   7                          6                        5                    4  3            2            1 0
    *        User Name Flag     Password Flag    Will Retain        Will QoS     Will Flag   Clean Session   Reserved
    */
    /* Username and password, and clean session unless the broker is to keep
     * the session. */
    kii->_mqtt_buffer[i++] = (char)(kii->_mqtt_clean_session == KII_FALSE ? 0xc0 : 0xc2);
    /* Variable header:Keep Alive, which kii_start_push_task() keeps to 16 bits */
    kii->_mqtt_buffer[i++] = (keep_alive_interval & 0xff00) >> 8;
    kii->_mqtt_buffer[i++] = keep_alive_interval & 0x00ff;
    /* Payload:Client Identifier */
    kii->_mqtt_buffer[i++] = (strlen(endpoint->topic) & 0xff00) >> 8;
    kii->_mqtt_buffer[i++] = strlen(endpoint->topic) & 0x00ff;
    strcpy(&(kii->_mqtt_buffer[i]), endpoint->topic);
    i += strlen(endpoint->topic);
    /* Payload:User Name */
    kii->_mqtt_buffer[i++] = (strlen(endpoint->username) & 0xff00) >> 8;
    kii->_mqtt_buffer[i++] = strlen(endpoint->username) & 0x00ff;
    strcpy(&(kii->_mqtt_buffer[i]), endpoint->username);
    i += strlen(endpoint->username);
    /* Payload:Password */
    kii->_mqtt_buffer[i++] = (strlen(endpoint->password) & 0xff00) >> 8;
    kii->_mqtt_buffer[i++] = strlen(endpoint->password) & 0x00ff;
    strcpy(&(kii->_mqtt_buffer[i]), endpoint->password);
    i += strlen(endpoint->password);

    int j = 0;
    /* Fixed header:byte1 */
    kii->_mqtt_buffer[j++] = 0x10;
    /*/ Fixed header:Remaining Length*/
    j += _mqtt_encode(&(kii->_mqtt_buffer[j]), i - 8);

    /* copy the other types */
    for(int k = 0; k < i - 8; k++)
    {
        kii->_mqtt_buffer[j++] = kii->_mqtt_buffer[8 + k];
    }

    ebisu_sock_code_t send_err = EBISU_SOCK_FAIL;
    size_t total_sent = 0;
    while (total_sent < j) {
        size_t sent_len = 0;
        send_err = kii->_mqtt_socket->ops->send(kii->_mqtt_socket,
                &kii->_mqtt_buffer[total_sent], j - total_sent, &sent_len);
        if (send_err == EBISU_SOCK_AGAIN || send_err == EBISU_SOCK_FAIL) {
            // Don't accept non-blocking socket when send_err is EBISU_SOCK_AGAIN.
            return EBISU_SOCK_FAIL;
        }
        total_sent += sent_len;
    }
    return send_err;
}

ebisu_sock_code_t _mqtt_send_subscribe(kii_t* kii, const char* topic, kii_mqtt_qos_t qos)
{
    memset(kii->_mqtt_buffer, 0, kii->_mqtt_buffer_size);
    int i = 8;

    /* Variable header:Packet Identifier */
    kii->_mqtt_buffer[i++] = 0x00;
    kii->_mqtt_buffer[i++] = 0x01;
    /* Payload:topic length */
    kii->_mqtt_buffer[i++] = (strlen(topic) & 0xff00) >> 8;
    kii->_mqtt_buffer[i++] = strlen(topic) & 0x00ff;
    /* Payload:topic */
    strcpy(&kii->_mqtt_buffer[i], topic);
    i += strlen(topic);
    /* Payload: qos*/
    kii->_mqtt_buffer[i++] = (char)qos;

    int j = 0;
    /* Fixed header: byte1*/
    kii->_mqtt_buffer[j++] = (char)0x82;
    /* Fixed header:Remaining Length*/
    j += _mqtt_encode(&(kii->_mqtt_buffer[j]), i - 8);

    /* copy the other types*/
    for(int k = 0; k < i - 8; k++)
    {
        kii->_mqtt_buffer[j++] = kii->_mqtt_buffer[8 + k];
    }

    ebisu_sock_code_t send_err = EBISU_SOCK_FAIL;
    size_t total_sent = 0;
    while (total_sent < j) {
        size_t sent_len = 0;
        send_err = kii->_mqtt_socket->ops->send(kii->_mqtt_socket,
                &kii->_mqtt_buffer[total_sent], j - total_sent, &sent_len);
        if (send_err == EBISU_SOCK_AGAIN || send_err == EBISU_SOCK_FAIL) {
            // Don't accept non-blocking socket when send_err is EBISU_SOCK_AGAIN.
            return EBISU_SOCK_FAIL;
        }
        total_sent += sent_len;
    }
    return send_err;
}

// Sends a packet that is a fixed header alone, with nothing after it: PINGREQ
// or DISCONNECT.
static ebisu_sock_code_t _mqtt_send_header_only(kii_t* kii, char byte1)
{
    char buff[2];
    buff[0] = byte1;
    buff[1] = 0x00;
    ebisu_sock_code_t sock_err = EBISU_SOCK_FAIL;
    size_t total_sent = 0;
    while (total_sent < sizeof(buff)) {
        size_t sent_len = 0;
        sock_err = kii->_mqtt_socket->ops->send(kii->_mqtt_socket, &buff[total_sent], sizeof(buff) - total_sent, &sent_len);
        if (sock_err == EBISU_SOCK_AGAIN || sock_err == EBISU_SOCK_FAIL) {
            // Don't accept non-blocking socket when send_err is EBISU_SOCK_AGAIN.
            return EBISU_SOCK_FAIL;
        }
        total_sent += sent_len;
    }
    return sock_err;
}

ebisu_sock_code_t _mqtt_send_pingreq(kii_t* kii)
{
    return _mqtt_send_header_only(kii, (char)0xc0);
}

static ebisu_sock_code_t _mqtt_send_disconnect(kii_t* kii)
{
    return _mqtt_send_header_only(kii, (char)0xe0);
}

// Reads the first byte of a fixed header, which holds the packet type, and
// returns what the recv callback returned, EBISU_SOCK_AGAIN included: RECV_READY
// handles a failure here, before any of a packet has been read, differently.
// Reads the first byte of a fixed header. EBISU_SOCK_OK with *out_received 0 is
// the end of the stream: the broker closed the connection.
static ebisu_sock_code_t _mqtt_recv_fixed_header_byte1(kii_t* kii, kii_mqtt_fixed_header* fixed_header, size_t* out_received)
{
    char c = '\0';
    *out_received = 0;
    ebisu_sock_code_t res = kii->_mqtt_socket->ops->recv(kii->_mqtt_socket, &c, 1, out_received);
    fixed_header->byte1 = c;
    return res;
}

// Reads the rest of a fixed header, the remaining length.
static ebisu_sock_code_t _mqtt_recv_fixed_header_rest(kii_t* kii, kii_mqtt_fixed_header* fixed_header)
{
    size_t received = 0;
    char buff[4];
    for (int i = 0; i < 4; ++i) {
        char c2 = '\0';
        ebisu_sock_code_t res = kii->_mqtt_socket->ops->recv(kii->_mqtt_socket, &c2, 1, &received);
        if (res == EBISU_SOCK_FAIL) {
            return EBISU_SOCK_FAIL;
        }
        else if (res == EBISU_SOCK_AGAIN) {
            // Don't accept non-blocking mode.
            return EBISU_SOCK_FAIL;
        }
        if (received == 0) {
            // End of stream: the broker closed the connection.
            return EBISU_SOCK_FAIL;
        }
        buff[i] = c2;
        if ((c2 & 128) == 0) { // Check continue bit.
            break;
        }
    }

    unsigned long remaining_length = 0;
    // MQTT allows four bytes. A fourth that says more follow leaves the stream
    // at a byte that is no packet's start.
    if (_mqtt_decode(buff, &remaining_length) < 0) {
        return EBISU_SOCK_FAIL;
    }

    fixed_header->remaining_length = remaining_length;
    return EBISU_SOCK_OK;
}

ebisu_sock_code_t _mqtt_recv_fixed_header(kii_t* kii, kii_mqtt_fixed_header* fixed_header)
{
    size_t received = 0;
    ebisu_sock_code_t res = _mqtt_recv_fixed_header_byte1(kii, fixed_header, &received);
    if (res != EBISU_SOCK_OK || received == 0) {
        // EBISU_SOCK_FAIL, EBISU_SOCK_AGAIN (don't accept non-blocking mode), or
        // the end of the stream.
        return EBISU_SOCK_FAIL;
    }
    return _mqtt_recv_fixed_header_rest(kii, fixed_header);
}

// Whether the MQTT recv callback returning EBISU_SOCK_FAIL just now means the
// connection is lost, rather than that the receive timeout ran out. It returns
// EBISU_SOCK_FAIL for both, so only the
// socket's recv_timed_out can tell. Without one, take it for a timeout, as on
// an idle connection the receive keeps timing out.
static kii_bool_t _mqtt_sock_recv_lost(kii_t* kii)
{
    ebisu_socket_t* sock = kii->_mqtt_socket;
    if (sock->ops->recv_timed_out == NULL) {
        return KII_FALSE;
    }
    return sock->ops->recv_timed_out(sock) ? KII_FALSE : KII_TRUE;
}

// If the MQTT buffer size is insufficient,
// read and trash the message so that the loop can wait for next message.
ebisu_sock_code_t _mqtt_recv_remaining_trash(kii_t* kii, unsigned long remaining_length) {
    enum { buff_size = 256 };
    char buff[buff_size];
    size_t received = 0;
    size_t total_received = 0;
    // A packet with nothing after its fixed header leaves nothing to discard.
    ebisu_sock_code_t res = EBISU_SOCK_OK;
    while (total_received < remaining_length) {
        // Ask for no more than is left of this packet. recv on a stream
        // socket returns whatever is there up to the size asked for, so asking
        // for more takes the start of the next packet too when it has arrived.
        size_t length_to_read = remaining_length - total_received;
        if (length_to_read > buff_size) {
            length_to_read = buff_size;
        }
        res = kii->_mqtt_socket->ops->recv(
                kii->_mqtt_socket,
                buff,
                length_to_read,
                &received);
        if (res == EBISU_SOCK_FAIL) {
            return EBISU_SOCK_FAIL;
        }
        else if (res == EBISU_SOCK_AGAIN) {
            // Don't accept non-blocking mode.
            return EBISU_SOCK_FAIL;
        }
        else if (res == EBISU_SOCK_OK) {
            if (received == 0) {
                // End of stream: the peer closed the connection.
                return EBISU_SOCK_FAIL;
            }
            total_received += received;
        }
    }
    return res;
}

ebisu_sock_code_t _mqtt_recv_remaining(kii_t* kii, unsigned long remaining_length, char* buff)
{
    size_t received = 0;
    size_t total_received = 0;
    ebisu_sock_code_t res = EBISU_SOCK_FAIL;
    while (total_received < remaining_length) {
        res = kii->_mqtt_socket->ops->recv(
                kii->_mqtt_socket,
                buff + total_received,
                remaining_length - total_received,
                &received);
        if (res == EBISU_SOCK_FAIL) {
            return EBISU_SOCK_FAIL;
        }
        else if (res == EBISU_SOCK_AGAIN) {
            // Don't accept non-blocking mode.
            return EBISU_SOCK_FAIL;
        }
        else if (res == EBISU_SOCK_OK) {
            if (received == 0) {
                // End of stream: the peer closed the connection.
                return EBISU_SOCK_FAIL;
            }
            total_received += received;
        }
    }
    return res;
}

// Whether the system has a monotonic clock, which then times the keep-alive.
// Without one, the time is estimated by adding up the waits and receive
// timeouts the task goes through, which misses the time spent elsewhere, such
// as in the push callback.
static kii_bool_t _has_clock(const kii_t* kii)
{
    return (kii->_system != NULL && kii->_system->ops->monotonic_ms != NULL) ?
        KII_TRUE : KII_FALSE;
}

// Starts the time since the last packet that counts for the keep-alive again,
// at ms.
static void _reset_elapsed(mqtt_state_t* state, uint32_t ms)
{
    kii_t* kii = state->kii;
    state->elapsed_time_ms = ms;
    if (_has_clock(kii) == KII_TRUE) {
        // Unsigned, so this wraps round and back again if the clock is below
        // ms, and the difference taken in _elapsed_ms() is still ms.
        state->elapsed_since_ms = kii->_system->ops->monotonic_ms(kii->_system) - ms;
    }
}

// The time since the last packet that counts for the keep-alive, in
// milliseconds, at most UINT32_MAX.
static uint32_t _elapsed_ms(const mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    if (_has_clock(kii) != KII_TRUE) {
        return state->elapsed_time_ms;
    }
    uint64_t ms = kii->_system->ops->monotonic_ms(kii->_system) - state->elapsed_since_ms;
    return ms > UINT32_MAX ? UINT32_MAX : (uint32_t)ms;
}

void _init_mqtt_state(kii_t* kii, mqtt_state_t* state)
{
    state->kii = kii;
    state->elapsed_since_ms = 0;
    memset(&state->endpoint, 0x00, sizeof(kii_mqtt_endpoint_t));
    state->info.error = KII_MQTT_ERR_OK;
    state->info.task_state = KII_MQTT_ST_INSTALL_PUSH;
    if (kii->_onboarded_thing_id[0] != '\0' &&
            strcmp(kii->_onboarded_thing_id, kii->_author.author_id) == 0) {
        // Onboarding installed push and returned the endpoint: connect with it.
        state->ins_id = kii->_onboarded_installation_id;
        state->endpoint = kii->_onboarded_endpoint;
        state->info.task_state = KII_MQTT_ST_SOCK_CONNECT;
    }
    _reset_elapsed(state, 0);
    state->remaining_message_size = 0;
    state->message_qos = 0;
    state->awaiting_suback = KII_FALSE;
    state->pingresp_received = KII_TRUE;
    state->sock_connected = KII_FALSE;
    state->mqtt_connected = KII_FALSE;
    state->retry_wait_ms = RETRY_WAIT_MIN_MS;
    state->wait_left_ms = 0;
    state->connect_failures = 0;
}

/* Waits longer each time, so that devices do not keep at a cloud that is
 * refusing them or down, and part of the way at random, so that the ones that
 * lost it together do not come back together. Nothing here has a source of
 * randomness, but the thing's ID differs from device to device, and it is
 * mixed with the wait so that a device's share changes as it goes. */
static void _schedule_retry(mqtt_state_t* state)
{
    const uint32_t max_ms = state->kii->_mqtt_retry_wait_max_ms != 0 ?
        state->kii->_mqtt_retry_wait_max_ms : RETRY_WAIT_MAX_MS;
    // A longest wait shorter than the first wait cuts that short too.
    uint32_t wait_ms = state->retry_wait_ms < max_ms ? state->retry_wait_ms : max_ms;
    // FNV-1a.
    uint32_t hash = 2166136261U;
    for (const char* c = state->kii->_author.author_id; *c != '\0'; ++c) {
        hash = (hash ^ (unsigned char)*c) * 16777619U;
    }
    hash = (hash ^ wait_ms) * 16777619U;

    state->wait_left_ms = wait_ms / 2 + hash % (wait_ms / 2 + 1);
    state->retry_wait_ms = (wait_ms > max_ms / 2) ? max_ms : wait_ms * 2;
}

/* Whether a request that failed may succeed if tried again: the server said so,
 * or it never answered, which is no reason to stop on a device that can lose
 * its network at any time. */
static kii_bool_t _is_temporary_error(kii_t* kii, kii_code_t res)
{
    switch (res) {
        case KII_ERR_SOCK_CONNECT:
        case KII_ERR_SOCK_CLOSE:
        case KII_ERR_SOCK_SEND:
        case KII_ERR_SOCK_RECV:
            return KII_TRUE;
        default:
            break;
    }
    uint16_t status_code = khc_get_status_code(&kii->_khc);
    return ((500 <= status_code && status_code < 600) || status_code == 429) ?
        KII_TRUE : KII_FALSE;
}

void _mqtt_state_install_push(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    kii_code_t res = kii_install_push(kii, KII_FALSE, &state->ins_id);
    if (res != KII_ERR_OK) {
        if (_is_temporary_error(kii, res) == KII_TRUE) {
            // Temporal error. Try again.
            _schedule_retry(state);
        } else {
            // Permanent error. Exit loop.
            task_info->error = KII_MQTT_ERR_INSTALLATION;
            task_info->task_state = KII_MQTT_ST_ERR_EXIT;
        }
    } else {
        task_info->task_state = KII_MQTT_ST_GET_ENDPOINT;
    }
}

/* Gets new MQTT credentials the way the thing's token is renewed: by onboarding
 * again through Thing-IF, which renews the token too. */
static kii_code_t _onboard_for_endpoint(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_code_t res = _kii_session_onboard_again(kii);
    if (res != KII_ERR_OK) {
        return res;
    }
    if (kii->_onboarded_thing_id[0] == '\0' ||
            strcmp(kii->_onboarded_thing_id, kii->_author.author_id) != 0) {
        /* Onboarding returned no endpoint: MQTT is not enabled. */
        return KII_ERR_FAIL;
    }
    state->ins_id = kii->_onboarded_installation_id;
    state->endpoint = kii->_onboarded_endpoint;
    return KII_ERR_OK;
}

void _mqtt_state_get_endpoint(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    _delay_ms(kii, WAIT_MS);
    kii_code_t res = (_kii_session_renews_by_onboarding(kii) == KII_TRUE) ?
        _onboard_for_endpoint(state) :
        kii_get_mqtt_endpoint(kii, state->ins_id.id, &state->endpoint);
    if (res != KII_ERR_OK) {
        if (_is_temporary_error(kii, res) == KII_TRUE) {
            // Temporal error. Try again.
            _schedule_retry(state);
        } else {
            task_info->error = KII_MQTT_ERR_GET_ENDPOINT;
            task_info->task_state = KII_MQTT_ST_ERR_EXIT;
        }
    } else {
        task_info->task_state = KII_MQTT_ST_SOCK_CONNECT;
    }
}

void _mqtt_state_sock_connect(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    kii_mqtt_endpoint_t* endpoint = &state->endpoint;
    uint16_t port = endpoint->port_ssl;
    if (kii->_insecure_mqtt == KII_TRUE) {
        port = endpoint->port_tcp;
    }

    ebisu_sock_code_t con_res =
        kii->_mqtt_socket->ops->connect(
                kii->_mqtt_socket,
                endpoint->host, port);
    if (con_res != EBISU_SOCK_OK) {
        _schedule_retry(state);
        if (++state->connect_failures >= MAX_CONNECT_FAILURES) {
            state->connect_failures = 0;
            task_info->task_state = KII_MQTT_ST_GET_ENDPOINT;
        }
    } else {
        state->connect_failures = 0;
        state->sock_connected = KII_TRUE;
        task_info->task_state = KII_MQTT_ST_SEND_CONNECT;
    }
}

void _mqtt_state_send_connect(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    kii_mqtt_endpoint_t* endpoint = &state->endpoint;
    if (kii->_mqtt_buffer == NULL || kii->_mqtt_buffer_size == 0) {
        task_info->error = KII_MQTT_ERR_INSUFFICIENT_BUFF;
        task_info->task_state = KII_MQTT_ST_ERR_EXIT;
        return;
    }
    // The 8 bytes kept for the fixed header, the variable header with the
    // protocol name, 2 bytes before each string, and a terminator after the
    // last, which it is copied with.
    if (kii->_mqtt_buffer_size < 21 + strlen(_mqtt_protocol_name(kii)) +
            strlen(endpoint->topic) +
            strlen(endpoint->username) + strlen(endpoint->password)) {
        task_info->error = KII_MQTT_ERR_INSUFFICIENT_BUFF;
        task_info->task_state = KII_MQTT_ST_ERR_EXIT;
        return;
    }
    ebisu_sock_code_t send_err = _mqtt_send_connect(kii, endpoint);
    if (send_err == EBISU_SOCK_FAIL) {
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
    } else {
        // The keep-alive counts from CONNECT. Before it, the clock may have
        // gone on through the waits between attempts to reconnect.
        _reset_elapsed(state, 0);
        task_info->task_state = KII_MQTT_ST_RECV_CONNACK;
    }
}

void _mqtt_state_recv_connack(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    kii_mqtt_fixed_header fixed_header;
    ebisu_sock_code_t res = _mqtt_recv_fixed_header(kii, &fixed_header);
    if (res != EBISU_SOCK_OK) {
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    char ptype = fixed_header.byte1 & 0xf0;
    if (ptype != 0x20) {
        // Must not happens. CONNACK must be the first response.
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    // CONNACK variable header LENGTH is always 2.
    enum { length = 2 };
    char buff[length];
    memset(buff, '\0', length);
    ebisu_sock_code_t res2 = _mqtt_recv_remaining(kii, length, buff);
    if (res2 != EBISU_SOCK_OK) {
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    char return_code = buff[1];
    if (return_code == 4 || return_code == 5) {
        // Bad user name or password, or not authorized: the broker refused the
        // credentials, e.g. because they expired or were revoked. RECONNECT
        // would present the same ones again and be refused forever, so close
        // the socket as RECONNECT does and get new ones: GET_ENDPOINT fetches
        // them for the same installation, and if it is no longer valid either,
        // fails permanently and exits; or, for a session that renews by
        // onboarding again, onboards again.
        kii->_mqtt_socket->ops->close(kii->_mqtt_socket);
        state->sock_connected = KII_FALSE;
        _schedule_retry(state);
        task_info->task_state = KII_MQTT_ST_GET_ENDPOINT;
    } else if (return_code != 0) {
        // CONNACK indicates failure.
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
    } else {
        // The connection is usable again, so the previous one's outstanding
        // PINGREQ no longer counts. Without this the first PINGREQ after any
        // reconnect would see the stale KII_FALSE and reconnect again, forever.
        // _init_mqtt_state() runs once, before the state loop, so it cannot do
        // this: RECONNECT goes to SOCK_CONNECT inside the loop.
        state->pingresp_received = KII_TRUE;
        state->mqtt_connected = KII_TRUE;
        task_info->task_state = KII_MQTT_ST_SEND_SUBSCRIBE;
    }
}

void _mqtt_state_send_subscribe(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    ebisu_sock_code_t res = _mqtt_send_subscribe(kii, state->endpoint.topic,
            kii->_mqtt_qos == KII_MQTT_QOS_1 ? KII_MQTT_QOS_1 : KII_MQTT_QOS_0);
    if (res == EBISU_SOCK_FAIL) {
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
    } else { // KHC_ERR_OK.
        task_info->task_state = KII_MQTT_ST_RECV_SUBACK;
    }
}

void _mqtt_state_recv_suback(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    state->awaiting_suback = KII_FALSE;
    kii_mqtt_fixed_header fixed_header;
    ebisu_sock_code_t res = _mqtt_recv_fixed_header(kii, &fixed_header);
    if (res != EBISU_SOCK_OK) {
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    unsigned char ptype = fixed_header.byte1 & 0xf0;
    if (ptype == 0x30) {
        // A broker may send messages before SUBACK, as one that kept the
        // session does, for the subscription it kept. Take the message, then
        // wait for SUBACK again.
        state->remaining_message_size = fixed_header.remaining_length;
        state->message_qos = ((unsigned char)fixed_header.byte1 >> 1) & 0x03;
        state->awaiting_suback = KII_TRUE;
        task_info->task_state = KII_MQTT_ST_RECV_MSG;
        return;
    }
    if (ptype != 0x90) {
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    enum { len = 3 };
    char buff[len];
    memset(buff, '\0', len);
    ebisu_sock_code_t res2 = _mqtt_recv_remaining(kii, len, buff);
    if (res2 != EBISU_SOCK_OK) {
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    if (buff[0] == 0x00 && // Identifier MSB
            buff[1] == 0x01 && // Identifier LSB
            (unsigned char)buff[2] != 0x80) // Return Code
    {
        // Connected and subscribed: losing this is a new failure.
        state->retry_wait_ms = RETRY_WAIT_MIN_MS;
        task_info->task_state = KII_MQTT_ST_RECV_READY;
    } else {
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
    }
}

// Adds to the time elapsed since the last PINGREQ, stopping at UINT32_MAX
// rather than wrapping round to a time at which no PINGREQ is due.
static void _add_elapsed_ms(mqtt_state_t* state, uint32_t ms)
{
    if (ms > UINT32_MAX - state->elapsed_time_ms) {
        state->elapsed_time_ms = UINT32_MAX;
    } else {
        state->elapsed_time_ms += ms;
    }
}

// The receive timeout in milliseconds, or UINT32_MAX if it is longer than that.
static uint32_t _to_recv_ms(const kii_t* kii)
{
    if (kii->_mqtt_to_recv_sec > UINT32_MAX / 1000) {
        return UINT32_MAX;
    }
    return kii->_mqtt_to_recv_sec * 1000;
}

void _mqtt_state_recv_ready(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    // A keep-alive of 0 disables PINGREQ. Otherwise send one early enough that
    // a receive running into its timeout still ends within the keep-alive.
    // A timeout as long as the keep-alive leaves no such margin. Then send one
    // once the keep-alive is over, which is after any receive that times out,
    // rather than at once: that would come back here straight after sending
    // it, before its PINGRESP could be read, and reconnect for want of it.
    const uint32_t keep_alive = kii->_keep_alive_interval;
    const uint32_t to_recv = kii->_mqtt_to_recv_sec;
    // The keep-alive is a uint16_t, so this is at most 65535000 and fits.
    const uint32_t thresh_ms =
        (to_recv < keep_alive ? keep_alive - to_recv : keep_alive) * 1000;
    if (keep_alive > 0 && _elapsed_ms(state) > thresh_ms) {
        task_info->task_state = KII_MQTT_ST_SEND_PINGREQ;
        return;
    }
    kii_mqtt_fixed_header fh;
    size_t received = 0;
    ebisu_sock_code_t res = _mqtt_recv_fixed_header_byte1(kii, &fh, &received);
    if (res == EBISU_SOCK_OK && received == 0) {
        // End of stream: the broker closed the connection.
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    if (res == EBISU_SOCK_OK) {
        res = _mqtt_recv_fixed_header_rest(kii, &fh);
        if (res != EBISU_SOCK_OK) {
            // A packet has started to arrive, so the next read would begin
            // inside it and take its bytes for a fixed header. Reconnect,
            // whether the receive timed out or failed, as RECV_MSG does.
            _reset_elapsed(state, 0);
            _delay_ms(kii, WAIT_MS);
            task_info->task_state = KII_MQTT_ST_RECONNECT;
            return;
        }
        unsigned char mtype = fh.byte1 & 0xf0;
        if (mtype == 0x30) { // PUBLISH
            state->remaining_message_size = fh.remaining_length;
            state->message_qos = ((unsigned char)fh.byte1 >> 1) & 0x03;
            task_info->task_state = KII_MQTT_ST_RECV_MSG;
            // Estimate worst case.
            _add_elapsed_ms(state, _to_recv_ms(kii));
        } else if (mtype == 0xD0) { // PINGRESP
            state->pingresp_received = KII_TRUE;
            _add_elapsed_ms(state, ARRIVED_MSG_READ_TIME);
        } else { // Ignore other messages.
            unsigned long size = fh.remaining_length;
            // Read and ignore.
            res = _mqtt_recv_remaining_trash(kii, size);
            if (res != EBISU_SOCK_OK) {
                _reset_elapsed(state, 0);
                _delay_ms(kii, WAIT_MS);
                task_info->task_state = KII_MQTT_ST_RECONNECT;
            }
        }
    } else if (res == EBISU_SOCK_FAIL && _mqtt_sock_recv_lost(kii) == KII_TRUE) {
        // Waiting on would wait for a connection that is gone.
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
    } else {
        // No packet has started to arrive: the receive timed out, or returned
        // EBISU_SOCK_AGAIN, as it would in non-blocking mode.
        // Just repeat same state after the wait.
        _add_elapsed_ms(state, _to_recv_ms(kii));
        _add_elapsed_ms(state, WAIT_MS);
        _delay_ms(kii, WAIT_MS);
    }
}

// Sends PUBACK, which acknowledges the QoS 1 message with packet identifier id.
static ebisu_sock_code_t _mqtt_send_puback(kii_t* kii, uint16_t id)
{
    char buff[4];
    buff[0] = (char)0x40;
    buff[1] = 0x02;
    buff[2] = (char)(id >> 8);
    buff[3] = (char)(id & 0xff);
    ebisu_sock_code_t sock_err = EBISU_SOCK_FAIL;
    size_t total_sent = 0;
    while (total_sent < sizeof(buff)) {
        size_t sent_len = 0;
        sock_err = kii->_mqtt_socket->ops->send(kii->_mqtt_socket, &buff[total_sent], sizeof(buff) - total_sent, &sent_len);
        if (sock_err == EBISU_SOCK_AGAIN || sock_err == EBISU_SOCK_FAIL) {
            // Don't accept non-blocking socket when send_err is EBISU_SOCK_AGAIN.
            return EBISU_SOCK_FAIL;
        }
        total_sent += sent_len;
    }
    return sock_err;
}

// Reads and drops a message larger than the buffer. Of one at QoS 1, it reads
// the packet identifier on the way, and says so, for the message to be
// acknowledged all the same: the broker would send it again and again, and it
// would never fit.
static ebisu_sock_code_t _mqtt_recv_msg_trash(mqtt_state_t* state,
        kii_bool_t* out_has_id, uint16_t* out_id)
{
    kii_t* kii = state->kii;
    unsigned long left = state->remaining_message_size;
    *out_has_id = KII_FALSE;
    if (state->message_qos == 0 || left < 2) {
        return _mqtt_recv_remaining_trash(kii, left);
    }
    char two[2];
    ebisu_sock_code_t res = _mqtt_recv_remaining(kii, 2, two);
    if (res != EBISU_SOCK_OK) {
        return res;
    }
    left -= 2;
    unsigned long topic_size = (unsigned char)two[0] * 256 + (unsigned char)two[1];
    if (topic_size + 2 > left) {
        // No room for a packet identifier after the topic.
        return _mqtt_recv_remaining_trash(kii, left);
    }
    res = _mqtt_recv_remaining_trash(kii, topic_size);
    if (res != EBISU_SOCK_OK) {
        return res;
    }
    res = _mqtt_recv_remaining(kii, 2, two);
    if (res != EBISU_SOCK_OK) {
        return res;
    }
    res = _mqtt_recv_remaining_trash(kii, left - topic_size - 2);
    if (res != EBISU_SOCK_OK) {
        return res;
    }
    *out_has_id = KII_TRUE;
    *out_id = (uint16_t)((unsigned char)two[0] * 256 + (unsigned char)two[1]);
    return EBISU_SOCK_OK;
}

void _mqtt_state_recv_msg(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    const unsigned long remaining_message_size = state->remaining_message_size;
    // Whether the message is at QoS 1, and so to be acknowledged, and with
    // which packet identifier.
    kii_bool_t has_id = KII_FALSE;
    uint16_t id = 0;
    if (state->message_qos > 1) {
        // QoS 2, which the task never subscribes with, or 3, which there is
        // none of: the broker has broken the protocol, and MQTT has the client
        // close the connection.
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    if (remaining_message_size > kii->_mqtt_buffer_size) {
        // Ignore message.
        ebisu_sock_code_t res = _mqtt_recv_msg_trash(state, &has_id, &id);
        if (res != EBISU_SOCK_OK) {
            _reset_elapsed(state, 0);
            _delay_ms(kii, WAIT_MS);
            task_info->task_state = KII_MQTT_ST_RECONNECT;
            return;
        }
        _add_elapsed_ms(state, ARRIVED_MSG_READ_TIME + WAIT_MS);
        _delay_ms(kii, WAIT_MS);
    } else {
        memset(kii->_mqtt_buffer, '\0', kii->_mqtt_buffer_size);
        ebisu_sock_code_t res = _mqtt_recv_remaining(kii, remaining_message_size, kii->_mqtt_buffer);
        if (res != EBISU_SOCK_OK) {
            _reset_elapsed(state, 0);
            _delay_ms(kii, WAIT_MS);
            task_info->task_state = KII_MQTT_ST_RECONNECT;
            return;
        }
        _add_elapsed_ms(state, ARRIVED_MSG_READ_TIME);
        // 2 bytes: topic size, then the topic, and at QoS 1 a packet
        // identifier of 2 bytes, then the body.
        // The topic size comes from the message: read its bytes unsigned,
        // as char may be signed, and check the topic and the packet identifier
        // fit before taking the rest as the body. A message where they do not
        // has been read whole all the same, so drop it, as one larger than the
        // buffer is.
        if (remaining_message_size >= 2) {
            unsigned int topic_size = (unsigned char)kii->_mqtt_buffer[0] * 256 + (unsigned char)kii->_mqtt_buffer[1];
            const unsigned int id_size = state->message_qos == 1 ? 2 : 0;
            if (topic_size + id_size <= remaining_message_size - 2) {
                char* id_ptr = kii->_mqtt_buffer + topic_size + 2;
                if (id_size > 0) {
                    has_id = KII_TRUE;
                    id = (uint16_t)((unsigned char)id_ptr[0] * 256 + (unsigned char)id_ptr[1]);
                }
                char* body_ptr = id_ptr + id_size;
                size_t body_length = remaining_message_size - topic_size - 2 - id_size;
                kii->_cb_push_received(body_ptr, body_length, kii->_push_data);
            }
        }
    }
    // Once the push callback has returned, so that a message lost on its way to
    // the application is sent again.
    if (has_id == KII_TRUE && _mqtt_send_puback(kii, id) != EBISU_SOCK_OK) {
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    task_info->task_state = state->awaiting_suback == KII_TRUE ?
        KII_MQTT_ST_RECV_SUBACK : KII_MQTT_ST_RECV_READY;
}

void _mqtt_state_send_pingreq(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;

    // The previous PINGREQ was never answered, so the connection is gone even
    // though sending may still appear to succeed. Reconnect rather than send
    // another and wait another keep-alive interval to find out.
    if (state->pingresp_received != KII_TRUE) {
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }

    ebisu_sock_code_t res = _mqtt_send_pingreq(kii);
    if (res != EBISU_SOCK_OK) {
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
    } else {
        state->pingresp_received = KII_FALSE;
        _reset_elapsed(state, MSG_SEND_TIME);
        task_info->task_state = KII_MQTT_ST_RECV_READY;
    }
}

void _mqtt_state_reconnect(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii->_mqtt_socket->ops->close(kii->_mqtt_socket);
    state->sock_connected = KII_FALSE;
    state->mqtt_connected = KII_FALSE;
    _schedule_retry(state);
    state->info.task_state = KII_MQTT_ST_SOCK_CONNECT;
}

const MQTT_STATE_HANDLER mqtt_state_handlers[] = {
    _mqtt_state_install_push,
    _mqtt_state_get_endpoint,
    _mqtt_state_sock_connect,
    _mqtt_state_send_connect,
    _mqtt_state_recv_connack,
    _mqtt_state_send_subscribe,
    _mqtt_state_recv_suback,
    _mqtt_state_recv_ready,
    _mqtt_state_recv_msg,
    _mqtt_state_send_pingreq,
    _mqtt_state_reconnect,
};

void* mqtt_start_task(void* sdata)
{
    kii_t* kii = (kii_t*)sdata;
    mqtt_state_t state;
    _init_mqtt_state(kii, &state);

    while(state.info.task_state < KII_MQTT_ST_ERR_EXIT) {
        if (kii->_cb_task_continue != NULL) {
            kii_bool_t cont = kii->_cb_task_continue(&state.info, kii->_task_continue_data);
            if (cont != KII_TRUE) {
                state.info.task_state = KII_MQTT_ST_DISCONTINUED;
                break;
            }
        }
        if (state.wait_left_ms > 0) {
            // A second at a time, so that the application can stop the task
            // however long the wait.
            uint32_t wait_ms = state.wait_left_ms < WAIT_MS ? state.wait_left_ms : WAIT_MS;
            _delay_ms(kii, wait_ms);
            state.wait_left_ms -= wait_ms;
            continue;
        }
        mqtt_state_handlers[state.info.task_state](&state);
    }
    // Discontinued, or exiting on an error, with a connection open: close it,
    // as nothing else will. The application cannot tell whether one is open.
    if (state.sock_connected == KII_TRUE) {
        // The broker accepted the connection, so say DISCONNECT, which tells
        // it the client is leaving rather than lost: it can end the session at
        // once, rather than once the keep-alive runs out. A reconnect doesn't,
        // as the connection it leaves is broken or refused. Whether it is sent
        // makes no difference to what follows.
        if (state.mqtt_connected == KII_TRUE) {
            _mqtt_send_disconnect(kii);
            state.mqtt_connected = KII_FALSE;
        }
        kii->_mqtt_socket->ops->close(kii->_mqtt_socket);
        state.sock_connected = KII_FALSE;
    }
    // Before the callback, which may start the task again.
    kii->_push_task_running = KII_FALSE;
    if (kii->_cb_task_exit != NULL) {
        kii->_cb_task_exit(&state.info, kii->_task_exit_data);
    }
    return NULL;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
