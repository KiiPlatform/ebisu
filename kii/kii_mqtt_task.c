#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "kii_mqtt_task.h"
#include <ebisu_mem.h>
#include "kii.h"
#include "kii_session_impl.h"

static void _delay_ms(kii_t* kii, uint32_t msec)
{
    kii->_system->ops->delay_ms(kii->_system, msec);
}

// Tells the event callback, if there is one.
static void _event(kii_t* kii, kii_mqtt_event_t event, uint32_t detail)
{
    if (kii->_cb_mqtt_event != NULL) {
        kii->_cb_mqtt_event(event, detail, kii->_mqtt_event_data);
    }
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
/* How often kii_mqtt_publish() looks at what the task has done with its
 * message. */
#define PUBLISH_POLL_MS 10
/* The longest remaining length four bytes can carry. */
#define MQTT_REMAINING_LENGTH_MAX ((size_t)268435455UL)
/* A turn of 0 would have the task wait for a packet without ever waiting. */
#if KII_MQTT_RECV_WAIT_TURN_MS < 1
#error "KII_MQTT_RECV_WAIT_TURN_MS must be 1 or more"
#endif

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
    // In as few bytes as it takes, so a last byte of 0 is one too many: 0x80
    // 0x00 is 0 written twice as long as it need be.
    if (i > 1 && buf[i - 1] == 0x00) {
        return -1;
    }

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
     * the session, and the will, if there is one, with its QoS and retain. */
    const kii_mqtt_will_t* will = kii->_mqtt_will;
    unsigned char flags = kii->_mqtt_clean_session ? 0xc2 : 0xc0;
    if (will != NULL) {
        flags |= 0x04 | (will->qos == KII_MQTT_QOS_1 ? 0x08 : 0x00) |
            (will->retain ? 0x20 : 0x00);
    }
    kii->_mqtt_buffer[i++] = (char)flags;
    /* Variable header:Keep Alive, which kii_start_push_task() keeps to 16 bits */
    kii->_mqtt_buffer[i++] = (keep_alive_interval & 0xff00) >> 8;
    kii->_mqtt_buffer[i++] = keep_alive_interval & 0x00ff;
    /* Payload:Client Identifier, the topic unless one is set */
    const char* client_id = kii->_mqtt_client_id != NULL ? kii->_mqtt_client_id : endpoint->topic;
    kii->_mqtt_buffer[i++] = (strlen(client_id) & 0xff00) >> 8;
    kii->_mqtt_buffer[i++] = strlen(client_id) & 0x00ff;
    strcpy(&(kii->_mqtt_buffer[i]), client_id);
    i += strlen(client_id);
    if (will != NULL) {
        /* Payload:Will Topic */
        const size_t will_topic_len = strlen(will->topic);
        kii->_mqtt_buffer[i++] = (will_topic_len & 0xff00) >> 8;
        kii->_mqtt_buffer[i++] = will_topic_len & 0x00ff;
        memcpy(&kii->_mqtt_buffer[i], will->topic, will_topic_len);
        i += will_topic_len;
        /* Payload:Will Message */
        kii->_mqtt_buffer[i++] = (will->message_length & 0xff00) >> 8;
        kii->_mqtt_buffer[i++] = will->message_length & 0x00ff;
        if (will->message_length > 0) {
            memcpy(&kii->_mqtt_buffer[i], will->message, will->message_length);
        }
        i += will->message_length;
    }
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
            send_err = EBISU_SOCK_FAIL;
            break;
        }
        total_sent += sent_len;
    }
    // The packet, where it was built and where it was sent from, carries the
    // password.
    ebisu_secure_zero(kii->_mqtt_buffer, (size_t)i);
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

// Sends all of data, in as many sends as the socket takes.
static ebisu_sock_code_t _mqtt_send_all(kii_t* kii, const char* data, size_t length)
{
    size_t total_sent = 0;
    while (total_sent < length) {
        size_t sent_len = 0;
        ebisu_sock_code_t sock_err = kii->_mqtt_socket->ops->send(kii->_mqtt_socket,
                data + total_sent, length - total_sent, &sent_len);
        if (sock_err != EBISU_SOCK_OK) {
            // Don't accept non-blocking socket when sock_err is EBISU_SOCK_AGAIN.
            return EBISU_SOCK_FAIL;
        }
        total_sent += sent_len;
    }
    return EBISU_SOCK_OK;
}

// A packet on its way out through the MQTT buffer: what is put in it is sent
// whenever it is full, and what is left when flushed, so that a packet that
// fits takes one send, and one that doesn't needn't fit.
typedef struct {
    kii_t* kii;
    size_t used;
    ebisu_sock_code_t res;
} mqtt_out_t;

static void _mqtt_out_flush(mqtt_out_t* out)
{
    if (out->res == EBISU_SOCK_OK && out->used > 0) {
        out->res = _mqtt_send_all(out->kii, out->kii->_mqtt_buffer, out->used);
    }
    out->used = 0;
}

static void _mqtt_out_put(mqtt_out_t* out, const char* data, size_t length)
{
    kii_t* kii = out->kii;
    while (length > 0 && out->res == EBISU_SOCK_OK) {
        if (out->used == kii->_mqtt_buffer_size) {
            _mqtt_out_flush(out);
        }
        size_t n = kii->_mqtt_buffer_size - out->used;
        if (n > length) {
            n = length;
        }
        memcpy(kii->_mqtt_buffer + out->used, data, n);
        out->used += n;
        data += n;
        length -= n;
    }
}

// Sends the PUBLISH of message, which kii_mqtt_publish() checked MQTT can
// carry.
static ebisu_sock_code_t _mqtt_send_publish(kii_t* kii, const kii_mqtt_message_t* message)
{
    const size_t id_size = message->qos == KII_MQTT_QOS_1 ? 2 : 0;
    // The fixed header, of up to five bytes, and the topic's length.
    char head[7];
    size_t i = 0;
    head[i++] = (char)(0x30 | (message->dup ? 0x08 : 0x00) |
            (message->qos == KII_MQTT_QOS_1 ? 0x02 : 0x00) | (message->retain ? 0x01 : 0x00));
    i += _mqtt_encode(&head[i], 2 + message->topic_length + id_size + message->payload_length);
    head[i++] = (char)(message->topic_length >> 8);
    head[i++] = (char)(message->topic_length & 0xff);
    char id[2];
    id[0] = (char)(message->packet_id >> 8);
    id[1] = (char)(message->packet_id & 0xff);

    mqtt_out_t out;
    out.kii = kii;
    out.used = 0;
    out.res = EBISU_SOCK_OK;
    _mqtt_out_put(&out, head, i);
    _mqtt_out_put(&out, message->topic, message->topic_length);
    _mqtt_out_put(&out, id, id_size);
    _mqtt_out_put(&out, message->payload, message->payload_length);
    _mqtt_out_flush(&out);
    return out.res;
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
static bool _mqtt_sock_recv_lost(kii_t* kii)
{
    ebisu_socket_t* sock = kii->_mqtt_socket;
    if (sock->ops->recv_timed_out == NULL) {
        return false;
    }
    return !sock->ops->recv_timed_out(sock);
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
// as in the message callback.
static bool _has_clock(const kii_t* kii)
{
    return (kii->_system != NULL && kii->_system->ops->monotonic_ms != NULL);
}

// Starts the time since the last packet that counts for the keep-alive again,
// at ms.
static void _reset_elapsed(mqtt_state_t* state, uint32_t ms)
{
    kii_t* kii = state->kii;
    state->elapsed_time_ms = ms;
    if (_has_clock(kii)) {
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
    if (!_has_clock(kii)) {
        return state->elapsed_time_ms;
    }
    uint64_t ms = kii->_system->ops->monotonic_ms(kii->_system) - state->elapsed_since_ms;
    return ms > UINT32_MAX ? UINT32_MAX : (uint32_t)ms;
}

// Ends a connection on which the broker sent a packet MQTT doesn't allow
// where it came, or one that isn't what its type says: MQTT has the client
// close the connection then, as nothing read after it can be trusted.
static void _refuse_packet(mqtt_state_t* state, kii_mqtt_event_t event, uint32_t detail)
{
    kii_t* kii = state->kii;
    _event(kii, event, detail);
    _reset_elapsed(state, 0);
    _delay_ms(kii, WAIT_MS);
    state->info.task_state = KII_MQTT_ST_RECONNECT;
}

// Whether a PUBLISH's topic is one MQTT 3.1.1 allows a broker to send: at
// least a character long, without the wildcards a subscription may have, and
// well-formed UTF-8 without U+0000 (sections 1.5.3, 3.3.2 and 4.7).
static bool _is_publish_topic(const char* topic, size_t size)
{
    size_t i = 0;
    if (size == 0) {
        return false;
    }
    while (i < size) {
        const unsigned char c = (unsigned char)topic[i];
        size_t len = 0;
        unsigned long cp = 0;
        unsigned long min = 0;
        if (c == '\0' || c == '+' || c == '#') {
            return false;
        } else if (c < 0x80) {
            ++i;
            continue;
        } else if ((c & 0xe0) == 0xc0) {
            len = 2;
            cp = c & 0x1f;
            min = 0x80;
        } else if ((c & 0xf0) == 0xe0) {
            len = 3;
            cp = c & 0x0f;
            min = 0x800;
        } else if ((c & 0xf8) == 0xf0) {
            len = 4;
            cp = c & 0x07;
            min = 0x10000;
        } else {
            return false;
        }
        if (size - i < len) {
            return false;
        }
        for (size_t j = 1; j < len; ++j) {
            const unsigned char cc = (unsigned char)topic[i + j];
            if ((cc & 0xc0) != 0x80) {
                return false;
            }
            cp = (cp << 6) | (cc & 0x3f);
        }
        // Not written longer than it takes, not a surrogate, and within
        // Unicode.
        if (cp < min || (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff) {
            return false;
        }
        i += len;
    }
    return true;
}

// What kii_mqtt_publish() and the task share is guarded by the lock set by
// kii_set_mqtt_lock(). Without one, no call can give the task a message, so the
// task finds none.
static void _pub_lock(kii_t* kii)
{
    kii->_mqtt_lock->ops->lock(kii->_mqtt_lock);
}

static void _pub_unlock(kii_t* kii)
{
    kii->_mqtt_lock->ops->unlock(kii->_mqtt_lock);
}

void _kii_mqtt_publish_open(kii_t* kii, bool open)
{
    if (kii->_mqtt_lock == NULL) {
        kii->_mqtt_pub_open = open;
        return;
    }
    _pub_lock(kii);
    kii->_mqtt_pub_open = open;
    _pub_unlock(kii);
}

// Whether a message is waiting to be sent.
static bool _publish_queued(kii_t* kii)
{
    if (kii->_mqtt_lock == NULL) {
        return false;
    }
    _pub_lock(kii);
    const bool queued = kii->_mqtt_pub_state == KII_MQTT_PUB_QUEUED;
    _pub_unlock(kii);
    return queued;
}

// Takes a message waiting to be sent, if there is one, so that its caller waits
// for the send to end.
static bool _publish_take(kii_t* kii)
{
    if (kii->_mqtt_lock == NULL) {
        return false;
    }
    _pub_lock(kii);
    const bool taken = kii->_mqtt_pub_state == KII_MQTT_PUB_QUEUED;
    if (taken) {
        kii->_mqtt_pub_state = KII_MQTT_PUB_SENDING;
    }
    _pub_unlock(kii);
    return taken;
}

// Whether the message sent at QoS 1 awaits a PUBACK, its caller there or not.
static bool _publish_awaits_puback(kii_t* kii)
{
    if (kii->_mqtt_lock == NULL) {
        return false;
    }
    _pub_lock(kii);
    const bool awaits = kii->_mqtt_pub_state == KII_MQTT_PUB_AWAITING_PUBACK ||
        kii->_mqtt_pub_state == KII_MQTT_PUB_ABANDONED;
    _pub_unlock(kii);
    return awaits;
}

// Whether a PUBACK with packet identifier id acknowledges the message sent at
// QoS 1, which is then done, or freed if its caller has gone.
static bool _publish_acked(kii_t* kii, uint16_t id)
{
    if (kii->_mqtt_lock == NULL) {
        return false;
    }
    _pub_lock(kii);
    const uint8_t pub_state = kii->_mqtt_pub_state;
    const bool acked = (pub_state == KII_MQTT_PUB_AWAITING_PUBACK ||
            pub_state == KII_MQTT_PUB_ABANDONED) && kii->_mqtt_pub.packet_id == id;
    if (acked && pub_state == KII_MQTT_PUB_AWAITING_PUBACK) {
        kii->_mqtt_pub_result = KII_ERR_OK;
        kii->_mqtt_pub_state = KII_MQTT_PUB_DONE;
    } else if (acked) {
        kii->_mqtt_pub_state = KII_MQTT_PUB_FREE;
    }
    _pub_unlock(kii);
    return acked;
}

// On a connection the broker accepted, a message at QoS 1 that the last one
// left unacknowledged is sent again, with DUP and its packet identifier, as
// MQTT has a client do on a session the broker kept: on a clean session too,
// as its caller waits for it to be acknowledged. One whose caller has gone is
// dropped.
static void _publish_reconnected(kii_t* kii)
{
    if (kii->_mqtt_lock == NULL) {
        return;
    }
    _pub_lock(kii);
    if (kii->_mqtt_pub_state == KII_MQTT_PUB_AWAITING_PUBACK) {
        kii->_mqtt_pub.dup = true;
        kii->_mqtt_pub_state = KII_MQTT_PUB_QUEUED;
    } else if (kii->_mqtt_pub_state == KII_MQTT_PUB_ABANDONED) {
        kii->_mqtt_pub_state = KII_MQTT_PUB_FREE;
    }
    _pub_unlock(kii);
}

// As the task ends: a message not yet done fails, and no call can give
// another until the task is started again.
static void _publish_close(kii_t* kii)
{
    if (kii->_mqtt_lock == NULL) {
        kii->_mqtt_pub_open = false;
        return;
    }
    _pub_lock(kii);
    kii->_mqtt_pub_open = false;
    const uint8_t pub_state = kii->_mqtt_pub_state;
    if (pub_state == KII_MQTT_PUB_QUEUED || pub_state == KII_MQTT_PUB_AWAITING_PUBACK) {
        kii->_mqtt_pub_result = KII_ERR_FAIL;
        kii->_mqtt_pub_state = KII_MQTT_PUB_DONE;
    } else if (pub_state == KII_MQTT_PUB_ABANDONED) {
        kii->_mqtt_pub_state = KII_MQTT_PUB_FREE;
    }
    _pub_unlock(kii);
}

void _init_mqtt_state(kii_t* kii, mqtt_state_t* state)
{
    state->kii = kii;
    state->elapsed_since_ms = 0;
    memset(&state->endpoint, 0x00, sizeof(kii_mqtt_endpoint_t));
    state->info.error = KII_MQTT_ERR_OK;
    state->info.task_state = KII_MQTT_ST_INSTALL_PUSH;
    if (kii->_mqtt_endpoint_given) {
        // The application's own: connect with it, nothing to install.
        state->endpoint = kii->_mqtt_endpoint;
        state->info.task_state = KII_MQTT_ST_SOCK_CONNECT;
    } else if (kii->_onboarded_thing_id[0] != '\0' &&
            strcmp(kii->_onboarded_thing_id, kii->_author.author_id) == 0) {
        // Onboarding installed push and returned the endpoint: connect with it.
        state->ins_id = kii->_onboarded_installation_id;
        state->endpoint = kii->_onboarded_endpoint;
        state->info.task_state = KII_MQTT_ST_SOCK_CONNECT;
    }
    _reset_elapsed(state, 0);
    state->remaining_message_size = 0;
    state->message_qos = 0;
    state->message_dup = false;
    state->message_retain = false;
    state->awaiting_suback = false;
    state->pingresp_received = true;
    state->sock_connected = false;
    state->mqtt_connected = false;
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
static bool _is_temporary_error(kii_t* kii, kii_code_t res)
{
    switch (res) {
        case KII_ERR_SOCK_CONNECT:
        case KII_ERR_SOCK_CLOSE:
        case KII_ERR_SOCK_SEND:
        case KII_ERR_SOCK_RECV:
        case KII_ERR_TIMEOUT:
            return true;
        default:
            break;
    }
    uint16_t status_code = khc_get_status_code(&kii->_khc);
    return ((500 <= status_code && status_code < 600) || status_code == 429);
}

void _mqtt_state_install_push(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    kii_code_t res = kii_install_push(kii, false, &state->ins_id);
    if (res != KII_ERR_OK) {
        _event(kii, KII_MQTT_EVENT_INSTALL_FAILED, khc_get_status_code(&kii->_khc));
        if (_is_temporary_error(kii, res)) {
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
    kii_code_t res = (_kii_session_renews_by_onboarding(kii)) ?
        _onboard_for_endpoint(state) :
        kii_get_mqtt_endpoint(kii, state->ins_id.id, &state->endpoint);
    if (res != KII_ERR_OK) {
        _event(kii, KII_MQTT_EVENT_ENDPOINT_FAILED, khc_get_status_code(&kii->_khc));
        if (_is_temporary_error(kii, res)) {
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
    if (kii->_insecure_mqtt) {
        port = endpoint->port_tcp;
    }

    ebisu_sock_code_t con_res =
        kii->_mqtt_socket->ops->connect(
                kii->_mqtt_socket,
                endpoint->host, port);
    if (con_res != EBISU_SOCK_OK) {
        _event(kii, KII_MQTT_EVENT_CONNECT_FAILED, state->connect_failures + 1);
        _schedule_retry(state);
        if (++state->connect_failures >= MAX_CONNECT_FAILURES) {
            state->connect_failures = 0;
            // An endpoint of the application's can't be asked for again: go
            // on trying it.
            if (!kii->_mqtt_endpoint_given) {
                task_info->task_state = KII_MQTT_ST_GET_ENDPOINT;
            }
        }
    } else {
        state->connect_failures = 0;
        state->sock_connected = true;
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
    // And the will's topic and message, with 2 bytes before each.
    const size_t will_size = kii->_mqtt_will == NULL ? 0 :
        4 + strlen(kii->_mqtt_will->topic) + kii->_mqtt_will->message_length;
    const char* client_id = kii->_mqtt_client_id != NULL ? kii->_mqtt_client_id : endpoint->topic;
    if (kii->_mqtt_buffer_size < 21 + strlen(_mqtt_protocol_name(kii)) +
            strlen(client_id) + will_size +
            strlen(endpoint->username) + strlen(endpoint->password)) {
        task_info->error = KII_MQTT_ERR_INSUFFICIENT_BUFF;
        task_info->task_state = KII_MQTT_ST_ERR_EXIT;
        return;
    }
    ebisu_sock_code_t send_err = _mqtt_send_connect(kii, endpoint);
    if (send_err == EBISU_SOCK_FAIL) {
        _event(kii, KII_MQTT_EVENT_SEND_FAILED, KII_MQTT_ST_SEND_CONNECT);
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
        _event(kii, KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_CONNACK);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    // CONNACK has to be the first packet, with no flags, and its variable
    // header is always 2 bytes: after one that says otherwise, what is read
    // next wouldn't be where the next packet starts.
    enum { length = 2 };
    if ((unsigned char)fixed_header.byte1 != 0x20 || fixed_header.remaining_length != length) {
        _event(kii, KII_MQTT_EVENT_UNEXPECTED_PACKET, (unsigned char)fixed_header.byte1);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    char buff[length];
    memset(buff, '\0', length);
    ebisu_sock_code_t res2 = _mqtt_recv_remaining(kii, length, buff);
    if (res2 != EBISU_SOCK_OK) {
        _event(kii, KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_CONNACK);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    char return_code = buff[1];
    // MQTT 3.1.1's acknowledge flags are Session Present alone, which only a
    // connection accepted for a session the broker was asked to keep may have
    // (section 3.2.2). MQTT 3.1 has a byte there that it doesn't use.
    const unsigned char ack_flags = (unsigned char)buff[0];
    if (kii->_mqtt_version != KII_MQTT_VERSION_3_1 && ((ack_flags & 0xfe) != 0 ||
            ((ack_flags & 0x01) != 0 && (return_code != 0 || kii->_mqtt_clean_session)))) {
        _refuse_packet(state, KII_MQTT_EVENT_UNEXPECTED_PACKET, 0x20);
        return;
    }
    _event(kii, KII_MQTT_EVENT_CONNACK, (unsigned char)return_code);
    if (return_code == 4 || return_code == 5) {
        // Bad user name or password, or not authorized: the broker refused the
        // credentials, e.g. because they expired or were revoked. RECONNECT
        // would present the same ones again and be refused forever, so close
        // the socket as RECONNECT does and get new ones: GET_ENDPOINT fetches
        // them for the same installation, and if it is no longer valid either,
        // fails permanently and exits; or, for a session that renews by
        // onboarding again, onboards again.
        kii->_mqtt_socket->ops->close(kii->_mqtt_socket);
        state->sock_connected = false;
        if (kii->_mqtt_endpoint_given) {
            // Kii Cloud has none for an endpoint of the application's: ask the
            // application, and end the task if it has none either.
            if (kii->_cb_mqtt_credentials != NULL &&
                    kii->_cb_mqtt_credentials(&state->endpoint, kii->_mqtt_credentials_data) &&
                    _kii_mqtt_endpoint_terminated(&state->endpoint)) {
                _schedule_retry(state);
                task_info->task_state = KII_MQTT_ST_SOCK_CONNECT;
            } else {
                task_info->error = KII_MQTT_ERR_CREDENTIALS;
                task_info->task_state = KII_MQTT_ST_ERR_EXIT;
            }
            return;
        }
        _schedule_retry(state);
        task_info->task_state = KII_MQTT_ST_GET_ENDPOINT;
    } else if (return_code != 0) {
        // CONNACK indicates failure.
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
    } else {
        // The connection is usable again, so the previous one's outstanding
        // PINGREQ no longer counts. Without this the first PINGREQ after any
        // reconnect would see the stale false and reconnect again, forever.
        // _init_mqtt_state() runs once, before the state loop, so it cannot do
        // this: RECONNECT goes to SOCK_CONNECT inside the loop.
        state->pingresp_received = true;
        state->mqtt_connected = true;
        _publish_reconnected(kii);
        task_info->task_state = KII_MQTT_ST_SEND_SUBSCRIBE;
    }
}

void _mqtt_state_send_subscribe(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    // The 8 bytes kept for the fixed header, the packet identifier, 2 bytes
    // before the topic, and the QoS after it, which takes the place of the
    // terminator the topic is copied with. CONNECT's check counts the topic
    // only when it is the client identifier.
    if (kii->_mqtt_buffer_size < 13 + strlen(state->endpoint.topic)) {
        task_info->error = KII_MQTT_ERR_INSUFFICIENT_BUFF;
        task_info->task_state = KII_MQTT_ST_ERR_EXIT;
        return;
    }
    ebisu_sock_code_t res = _mqtt_send_subscribe(kii, state->endpoint.topic,
            kii->_mqtt_qos == KII_MQTT_QOS_1 ? KII_MQTT_QOS_1 : KII_MQTT_QOS_0);
    if (res == EBISU_SOCK_FAIL) {
        _event(kii, KII_MQTT_EVENT_SEND_FAILED, KII_MQTT_ST_SEND_SUBSCRIBE);
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
    state->awaiting_suback = false;
    kii_mqtt_fixed_header fixed_header;
    ebisu_sock_code_t res = _mqtt_recv_fixed_header(kii, &fixed_header);
    if (res != EBISU_SOCK_OK) {
        _event(kii, KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_SUBACK);
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
        state->message_dup = ((unsigned char)fixed_header.byte1 & 0x08) != 0;
        state->message_retain = ((unsigned char)fixed_header.byte1 & 0x01) != 0;
        state->awaiting_suback = true;
        task_info->task_state = KII_MQTT_ST_RECV_MSG;
        return;
    }
    // With no flags, the packet identifier, and a return code for the one
    // topic subscribed.
    enum { len = 3 };
    if ((unsigned char)fixed_header.byte1 != 0x90 || fixed_header.remaining_length != len) {
        _event(kii, KII_MQTT_EVENT_UNEXPECTED_PACKET, (unsigned char)fixed_header.byte1);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    char buff[len];
    memset(buff, '\0', len);
    ebisu_sock_code_t res2 = _mqtt_recv_remaining(kii, len, buff);
    if (res2 != EBISU_SOCK_OK) {
        _event(kii, KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_SUBACK);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    const unsigned char return_code = (unsigned char)buff[2];
    // The QoS SUBSCRIBE asked for, which the broker may grant or lower, but
    // not raise.
    const unsigned char asked = kii->_mqtt_qos == KII_MQTT_QOS_1 ? 0x01 : 0x00;
    if (buff[0] != 0x00 || buff[1] != 0x01 ||
            (return_code != 0x80 && return_code > asked)) {
        // For a SUBSCRIBE other than the task's, whose identifier is 1, or
        // with a return code that is reserved or grants more than was asked.
        _event(kii, KII_MQTT_EVENT_UNEXPECTED_PACKET, (unsigned char)fixed_header.byte1);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    _event(kii, KII_MQTT_EVENT_SUBACK, return_code);
    if (return_code != 0x80) {
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

// Waits for a packet to start arriving, a turn at a time, until the receive
// timeout runs out or a message is given to send, which the task then sends
// rather than wait on. Whether one is arriving, and how long it waited.
static bool _mqtt_wait_for_packet(kii_t* kii, uint32_t* out_waited_ms)
{
    ebisu_socket_t* sock = kii->_mqtt_socket;
    // 0 for none, as the socket has it.
    const uint32_t to_recv_ms = _to_recv_ms(kii);
    uint32_t waited_ms = 0;
    for (;;) {
        uint32_t turn_ms = KII_MQTT_RECV_WAIT_TURN_MS;
        if (to_recv_ms > 0 && to_recv_ms - waited_ms < turn_ms) {
            turn_ms = to_recv_ms - waited_ms;
        }
        if (sock->ops->recv_wait(sock, turn_ms)) {
            *out_waited_ms = waited_ms;
            return true;
        }
        waited_ms = waited_ms > UINT32_MAX - turn_ms ? UINT32_MAX : waited_ms + turn_ms;
        if (_publish_queued(kii) || (to_recv_ms > 0 && waited_ms >= to_recv_ms)) {
            *out_waited_ms = waited_ms;
            return false;
        }
    }
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
    if (_publish_take(kii)) {
        task_info->task_state = KII_MQTT_ST_SEND_PUBLISH;
        return;
    }
    // Where a message can be given, and the socket can wait for a packet
    // without receiving it, the message needn't wait for a receive that lasts
    // up to its timeout. What the task waited counts as a receive that timed
    // out would, with no wait after it, as it waited already.
    uint32_t waited_ms = 0;
    if (kii->_mqtt_lock != NULL && kii->_mqtt_socket->ops->recv_wait != NULL &&
            !_mqtt_wait_for_packet(kii, &waited_ms)) {
        _add_elapsed_ms(state, waited_ms);
        return;
    }
    kii_mqtt_fixed_header fh;
    size_t received = 0;
    ebisu_sock_code_t res = _mqtt_recv_fixed_header_byte1(kii, &fh, &received);
    if (res == EBISU_SOCK_OK && received == 0) {
        // End of stream: the broker closed the connection.
        _event(kii, KII_MQTT_EVENT_CLOSED_BY_BROKER, 0);
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
            _event(kii, KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_READY);
            _reset_elapsed(state, 0);
            _delay_ms(kii, WAIT_MS);
            task_info->task_state = KII_MQTT_ST_RECONNECT;
            return;
        }
        unsigned char mtype = fh.byte1 & 0xf0;
        if (mtype == 0x30) { // PUBLISH
            state->remaining_message_size = fh.remaining_length;
            state->message_qos = ((unsigned char)fh.byte1 >> 1) & 0x03;
            state->message_dup = ((unsigned char)fh.byte1 & 0x08) != 0;
            state->message_retain = ((unsigned char)fh.byte1 & 0x01) != 0;
            task_info->task_state = KII_MQTT_ST_RECV_MSG;
            // Estimate worst case.
            _add_elapsed_ms(state, _to_recv_ms(kii));
        } else if ((unsigned char)fh.byte1 == 0xD0 && fh.remaining_length == 0) {
            // PINGRESP, its fixed header alone.
            state->pingresp_received = true;
            _add_elapsed_ms(state, ARRIVED_MSG_READ_TIME);
        } else if ((unsigned char)fh.byte1 == 0x40 && fh.remaining_length == 2 &&
                _publish_awaits_puback(kii)) {
            // PUBACK, with the packet identifier of what it acknowledges.
            char id[2];
            if (_mqtt_recv_remaining(kii, sizeof(id), id) != EBISU_SOCK_OK) {
                _event(kii, KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_READY);
                _reset_elapsed(state, 0);
                _delay_ms(kii, WAIT_MS);
                task_info->task_state = KII_MQTT_ST_RECONNECT;
                return;
            }
            if (!_publish_acked(kii, (uint16_t)((unsigned char)id[0] * 256 + (unsigned char)id[1]))) {
                // Of no message the task sent and awaits it for.
                _refuse_packet(state, KII_MQTT_EVENT_UNEXPECTED_PACKET, (unsigned char)fh.byte1);
                return;
            }
            _add_elapsed_ms(state, ARRIVED_MSG_READ_TIME);
        } else {
            // Nothing else comes to the task, once its CONNACK and SUBACK have
            // come: another CONNACK or SUBACK, an acknowledgement of nothing
            // it sent, a packet only a client sends, or one of a reserved
            // type. Nor does a PINGRESP or PUBACK with flags, or of another
            // length, which would otherwise be read as the next packet.
            _refuse_packet(state, KII_MQTT_EVENT_UNEXPECTED_PACKET, (unsigned char)fh.byte1);
        }
    } else if (res == EBISU_SOCK_FAIL && _mqtt_sock_recv_lost(kii)) {
        // Waiting on would wait for a connection that is gone.
        _event(kii, KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_READY);
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
    } else {
        // No packet has started to arrive: the receive timed out, or returned
        // EBISU_SOCK_AGAIN, as it would in non-blocking mode.
        // Just repeat same state after the wait, unless a message given while
        // the task received is waiting to be sent.
        _add_elapsed_ms(state, _to_recv_ms(kii));
        if (!_publish_queued(kii)) {
            _add_elapsed_ms(state, WAIT_MS);
            _delay_ms(kii, WAIT_MS);
        }
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

// Reads and drops a message larger than the buffer. It reads the topic's
// length on the way, to tell one with no topic, or with no room in it for its
// topic and packet identifier, as malformed, which is read no further. Of one at
// QoS 1, it reads the packet identifier, for the message to be acknowledged all
// the same: the broker would send it again and again, and it would never fit.
// The topic itself is dropped unchecked.
static ebisu_sock_code_t _mqtt_recv_msg_trash(mqtt_state_t* state,
        bool* out_malformed, bool* out_has_id, uint16_t* out_id)
{
    kii_t* kii = state->kii;
    unsigned long left = state->remaining_message_size;
    const unsigned long id_size = state->message_qos == 1 ? 2 : 0;
    *out_malformed = true;
    *out_has_id = false;
    if (left < 2) {
        return EBISU_SOCK_OK;
    }
    char two[2];
    ebisu_sock_code_t res = _mqtt_recv_remaining(kii, 2, two);
    if (res != EBISU_SOCK_OK) {
        return res;
    }
    left -= 2;
    unsigned long topic_size = (unsigned char)two[0] * 256 + (unsigned char)two[1];
    if (topic_size == 0 || topic_size + id_size > left) {
        return EBISU_SOCK_OK;
    }
    res = _mqtt_recv_remaining_trash(kii, topic_size);
    if (res != EBISU_SOCK_OK) {
        return res;
    }
    left -= topic_size;
    if (id_size > 0) {
        res = _mqtt_recv_remaining(kii, 2, two);
        if (res != EBISU_SOCK_OK) {
            return res;
        }
        left -= 2;
        *out_id = (uint16_t)((unsigned char)two[0] * 256 + (unsigned char)two[1]);
        if (*out_id == 0) {
            return EBISU_SOCK_OK;
        }
        *out_has_id = true;
    }
    *out_malformed = false;
    return _mqtt_recv_remaining_trash(kii, left);
}

void _mqtt_state_recv_msg(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    const unsigned long remaining_message_size = state->remaining_message_size;
    // Whether the message is at QoS 1, and so to be acknowledged, and with
    // which packet identifier.
    bool has_id = false;
    uint16_t id = 0;
    if (state->message_qos > 1 || (state->message_qos == 0 && state->message_dup)) {
        // QoS 2, which the task never subscribes with, or 3, which there is
        // none of, or DUP at QoS 0, which no message sent again has (section
        // 3.3.1.1): the broker has broken the protocol, and MQTT has the
        // client close the connection.
        _refuse_packet(state, KII_MQTT_EVENT_UNEXPECTED_PACKET,
                0x30 | (state->message_dup ? 0x08 : 0x00) | (state->message_qos << 1));
        return;
    }
    if (remaining_message_size > kii->_mqtt_buffer_size) {
        // Ignore message.
        bool malformed = true;
        ebisu_sock_code_t res = _mqtt_recv_msg_trash(state, &malformed, &has_id, &id);
        if (res != EBISU_SOCK_OK) {
            _event(kii, KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_MSG);
            _reset_elapsed(state, 0);
            _delay_ms(kii, WAIT_MS);
            task_info->task_state = KII_MQTT_ST_RECONNECT;
            return;
        }
        if (malformed) {
            _refuse_packet(state, KII_MQTT_EVENT_MESSAGE_MALFORMED, (uint32_t)remaining_message_size);
            return;
        }
        _event(kii, KII_MQTT_EVENT_MESSAGE_TOO_LARGE, (uint32_t)remaining_message_size);
        _add_elapsed_ms(state, ARRIVED_MSG_READ_TIME + WAIT_MS);
        _delay_ms(kii, WAIT_MS);
    } else {
        memset(kii->_mqtt_buffer, '\0', kii->_mqtt_buffer_size);
        ebisu_sock_code_t res = _mqtt_recv_remaining(kii, remaining_message_size, kii->_mqtt_buffer);
        if (res != EBISU_SOCK_OK) {
            _event(kii, KII_MQTT_EVENT_RECV_FAILED, KII_MQTT_ST_RECV_MSG);
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
        // fit before taking the rest as the body. A message where they do not,
        // or whose topic or packet identifier MQTT doesn't allow, is malformed.
        bool malformed = true;
        const unsigned int id_size = state->message_qos == 1 ? 2 : 0;
        unsigned int topic_size = 0;
        if (remaining_message_size >= 2) {
            topic_size = (unsigned char)kii->_mqtt_buffer[0] * 256 + (unsigned char)kii->_mqtt_buffer[1];
            if (topic_size + id_size <= remaining_message_size - 2) {
                const char* id_ptr = kii->_mqtt_buffer + 2 + topic_size;
                if (id_size > 0) {
                    has_id = true;
                    id = (uint16_t)((unsigned char)id_ptr[0] * 256 + (unsigned char)id_ptr[1]);
                }
                malformed = !_is_publish_topic(kii->_mqtt_buffer + 2, topic_size) ||
                    (has_id && id == 0);
            }
        }
        if (malformed) {
            _refuse_packet(state, KII_MQTT_EVENT_MESSAGE_MALFORMED, (uint32_t)remaining_message_size);
            return;
        }
        kii_mqtt_message_t message;
        message.payload = kii->_mqtt_buffer + 2 + topic_size + id_size;
        message.payload_length = remaining_message_size - topic_size - 2 - id_size;
        // Moved back over its length, the topic is terminated two bytes short
        // of where it ended, so the packet identifier and the payload stay
        // where they are.
        memmove(kii->_mqtt_buffer, kii->_mqtt_buffer + 2, topic_size);
        kii->_mqtt_buffer[topic_size] = '\0';
        message.topic = kii->_mqtt_buffer;
        message.topic_length = topic_size;
        message.qos = state->message_qos == 1 ? KII_MQTT_QOS_1 : KII_MQTT_QOS_0;
        message.retain = state->message_retain;
        message.dup = state->message_dup;
        message.packet_id = id;
        kii->_cb_mqtt_message(&message, kii->_mqtt_message_data);
    }
    // Once the message callback has returned, so that a message lost on its
    // way to the application is sent again.
    if (has_id && _mqtt_send_puback(kii, id) != EBISU_SOCK_OK) {
        _event(kii, KII_MQTT_EVENT_SEND_FAILED, KII_MQTT_ST_RECV_MSG);
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    task_info->task_state = state->awaiting_suback ?
        KII_MQTT_ST_RECV_SUBACK : KII_MQTT_ST_RECV_READY;
}

void _mqtt_state_send_pingreq(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;

    // The previous PINGREQ was never answered, so the connection is gone even
    // though sending may still appear to succeed. Reconnect rather than send
    // another and wait another keep-alive interval to find out.
    if (!state->pingresp_received) {
        _event(kii, KII_MQTT_EVENT_PING_TIMEOUT, kii->_keep_alive_interval);
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }

    ebisu_sock_code_t res = _mqtt_send_pingreq(kii);
    if (res != EBISU_SOCK_OK) {
        _event(kii, KII_MQTT_EVENT_SEND_FAILED, KII_MQTT_ST_SEND_PINGREQ);
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
    } else {
        state->pingresp_received = false;
        _reset_elapsed(state, MSG_SEND_TIME);
        task_info->task_state = KII_MQTT_ST_RECV_READY;
    }
}

void _mqtt_state_send_publish(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii_mqtt_task_info* task_info = &state->info;
    // Taken by _publish_take(), so its caller waits, and leaves the message as
    // it is, while it is sent.
    const ebisu_sock_code_t res = _mqtt_send_publish(kii, &kii->_mqtt_pub);
    const bool qos1 = kii->_mqtt_pub.qos == KII_MQTT_QOS_1;
    _pub_lock(kii);
    if (res == EBISU_SOCK_OK && qos1) {
        kii->_mqtt_pub_state = KII_MQTT_PUB_AWAITING_PUBACK;
    } else if (qos1) {
        // Part of it may have reached the broker: send it again, as MQTT has
        // a client do, on the next connection.
        kii->_mqtt_pub.dup = true;
        kii->_mqtt_pub_state = KII_MQTT_PUB_QUEUED;
    } else {
        kii->_mqtt_pub_result = res == EBISU_SOCK_OK ? KII_ERR_OK : KII_ERR_SOCK_SEND;
        kii->_mqtt_pub_state = KII_MQTT_PUB_DONE;
    }
    _pub_unlock(kii);
    if (res != EBISU_SOCK_OK) {
        _event(kii, KII_MQTT_EVENT_SEND_FAILED, KII_MQTT_ST_SEND_PUBLISH);
        _reset_elapsed(state, 0);
        _delay_ms(kii, WAIT_MS);
        task_info->task_state = KII_MQTT_ST_RECONNECT;
        return;
    }
    _add_elapsed_ms(state, MSG_SEND_TIME);
    task_info->task_state = KII_MQTT_ST_RECV_READY;
}

void _mqtt_state_reconnect(mqtt_state_t* state)
{
    kii_t* kii = state->kii;
    kii->_mqtt_socket->ops->close(kii->_mqtt_socket);
    state->sock_connected = false;
    state->mqtt_connected = false;
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
    _mqtt_state_send_publish,
    _mqtt_state_reconnect,
};

void* mqtt_start_task(void* sdata)
{
    kii_t* kii = (kii_t*)sdata;
    mqtt_state_t state;
    _init_mqtt_state(kii, &state);

    while(state.info.task_state < KII_MQTT_ST_ERR_EXIT) {
        if (kii->_cb_task_continue != NULL) {
            bool cont = kii->_cb_task_continue(&state.info, kii->_task_continue_data);
            if (!cont) {
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
    if (state.sock_connected) {
        // The broker accepted the connection, so say DISCONNECT, which tells
        // it the client is leaving rather than lost: it can end the session at
        // once, rather than once the keep-alive runs out. A reconnect doesn't,
        // as the connection it leaves is broken or refused. Whether it is sent
        // makes no difference to what follows.
        if (state.mqtt_connected) {
            _mqtt_send_disconnect(kii);
            state.mqtt_connected = false;
        }
        kii->_mqtt_socket->ops->close(kii->_mqtt_socket);
        state.sock_connected = false;
    }
    // The task's copy of the endpoint, with its credentials.
    ebisu_secure_zero(&state.endpoint, sizeof(state.endpoint));
    // Before the callback, which may start the task again.
    _publish_close(kii);
    kii->_push_task_running = false;
    if (kii->_cb_task_exit != NULL) {
        kii->_cb_task_exit(&state.info, kii->_task_exit_data);
    }
    return NULL;
}

// How long kii_mqtt_publish() has waited: by the monotonic clock if the system
// has one, or by adding up its waits.
typedef struct {
    uint64_t start_ms;
    uint64_t waited_ms;
} publish_wait_t;

static uint64_t _publish_waited_ms(const kii_t* kii, const publish_wait_t* wait)
{
    if (_has_clock(kii)) {
        return kii->_system->ops->monotonic_ms(kii->_system) - wait->start_ms;
    }
    return wait->waited_ms;
}

static void _publish_wait(kii_t* kii, publish_wait_t* wait)
{
    _delay_ms(kii, PUBLISH_POLL_MS);
    wait->waited_ms += PUBLISH_POLL_MS;
}

kii_code_t kii_mqtt_publish(
        kii_t* kii,
        const char* topic,
        const char* payload,
        size_t payload_length,
        kii_mqtt_qos_t qos,
        bool retain,
        uint32_t timeout_ms)
{
    if (kii->_mqtt_lock == NULL || kii->_system == NULL ||
            kii->_system->ops->delay_ms == NULL) {
        return KII_ERR_FAIL;
    }
    if ((qos != KII_MQTT_QOS_0 && qos != KII_MQTT_QOS_1) || topic == NULL ||
            (payload == NULL && payload_length > 0)) {
        return KII_ERR_FAIL;
    }
    const size_t topic_length = strlen(topic);
    if (topic_length > 65535) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    if (!_is_publish_topic(topic, topic_length)) {
        return KII_ERR_FAIL;
    }
    // The topic's length, the topic and, at QoS 1, the packet identifier come
    // before the payload, in the remaining length.
    const size_t id_size = qos == KII_MQTT_QOS_1 ? 2 : 0;
    if (payload_length > MQTT_REMAINING_LENGTH_MAX - 2 - topic_length - id_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }

    publish_wait_t wait;
    wait.start_ms = _has_clock(kii) ? kii->_system->ops->monotonic_ms(kii->_system) : 0;
    wait.waited_ms = 0;
    // Gives the task the message, once the one before is done with.
    for (;;) {
        _pub_lock(kii);
        if (!kii->_mqtt_pub_open) {
            _pub_unlock(kii);
            return KII_ERR_FAIL;
        }
        if (kii->_mqtt_pub_state == KII_MQTT_PUB_FREE) {
            kii_mqtt_message_t* message = &kii->_mqtt_pub;
            message->topic = topic;
            message->topic_length = topic_length;
            message->payload = payload;
            message->payload_length = payload_length;
            message->qos = qos;
            message->retain = retain;
            message->dup = false;
            message->packet_id = 0;
            if (qos == KII_MQTT_QOS_1) {
                // Not 0, which MQTT doesn't allow, nor 1, which SUBSCRIBE
                // has.
                const uint16_t last = kii->_mqtt_pub_last_id;
                message->packet_id = (last < 2 || last == 65535) ? 2 : (uint16_t)(last + 1);
                kii->_mqtt_pub_last_id = message->packet_id;
            }
            kii->_mqtt_pub_state = KII_MQTT_PUB_QUEUED;
            _pub_unlock(kii);
            break;
        }
        _pub_unlock(kii);
        if (_publish_waited_ms(kii, &wait) >= timeout_ms) {
            return KII_ERR_TIMEOUT;
        }
        _publish_wait(kii, &wait);
    }
    // Waits for the task to be done with it.
    for (;;) {
        const bool timed_out = _publish_waited_ms(kii, &wait) >= timeout_ms;
        _pub_lock(kii);
        const uint8_t pub_state = kii->_mqtt_pub_state;
        if (pub_state == KII_MQTT_PUB_DONE ||
                (timed_out && pub_state == KII_MQTT_PUB_QUEUED)) {
            const kii_code_t res = pub_state == KII_MQTT_PUB_DONE ?
                kii->_mqtt_pub_result : KII_ERR_TIMEOUT;
            kii->_mqtt_pub_state = KII_MQTT_PUB_FREE;
            kii->_mqtt_pub.topic = NULL;
            kii->_mqtt_pub.payload = NULL;
            _pub_unlock(kii);
            return res;
        }
        if (timed_out && pub_state == KII_MQTT_PUB_AWAITING_PUBACK) {
            // Its PUBACK may still come, which is then no surprise.
            kii->_mqtt_pub_state = KII_MQTT_PUB_ABANDONED;
            kii->_mqtt_pub.topic = NULL;
            kii->_mqtt_pub.payload = NULL;
            _pub_unlock(kii);
            return KII_ERR_TIMEOUT;
        }
        // Being sent, which reads topic and payload, or not yet timed out.
        _pub_unlock(kii);
        _publish_wait(kii, &wait);
    }
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
