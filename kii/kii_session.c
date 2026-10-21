#include <stdio.h>
#include <string.h>

#include "kii.h"
#include "kii_impl.h"
#include "kii_session_impl.h"
#include "kii_thing_impl.h"
#include "jkii_wrapper.h"

/* Kii's own SDKs renew five minutes before expiry. Short-lived tokens get a
 * fifth of their lifetime instead, so they are not renewed as soon as issued. */
#define _RENEW_MARGIN_MAX_MS ((uint64_t)5 * 60 * 1000)

/* Lifetimes beyond this, some 136 years, are treated as unknown rather than
 * scheduled. Kii's default, 2147483647 seconds, is within it. */
#define _LIFETIME_MAX_SEC ((int64_t)UINT32_MAX)

static const char _AUTH_HEADER_PREFIX[] = "Authorization: Bearer ";

/* The errorCode of the 403 Kii answers for a token that is not valid: one that
 * has expired, or been replaced by a renewal. */
static const char _WRONG_TOKEN[] = "WRONG_TOKEN";

void kii_session_init(kii_session_t* session)
{
    memset(session, 0x00, sizeof(kii_session_t));
    session->_renew_at_known = KII_FALSE;
}

void kii_session_set_cb_lock(
        kii_session_t* session,
        KII_CB_LOCK cb_lock,
        KII_CB_LOCK cb_unlock,
        void* userdata)
{
    session->_cb_lock = cb_lock;
    session->_cb_unlock = cb_unlock;
    session->_lock_data = userdata;
}

void kii_session_set_cb_monotonic_ms(
        kii_session_t* session,
        KII_CB_MONOTONIC_MS cb_monotonic_ms,
        void* userdata)
{
    session->_cb_monotonic_ms = cb_monotonic_ms;
    session->_monotonic_ms_data = userdata;
}

void kii_session_set_password(
        kii_session_t* session,
        const char* vendor_thing_id,
        const char* password)
{
    session->_vendor_thing_id = vendor_thing_id;
    session->_password = password;
}

void kii_session_set_cb_author_updated(
        kii_session_t* session,
        KII_CB_AUTHOR_UPDATED cb_author_updated,
        void* userdata)
{
    session->_cb_author_updated = cb_author_updated;
    session->_author_updated_data = userdata;
}

static kii_bool_t _has_lock(kii_session_t* session)
{
    return (session->_cb_lock != NULL && session->_cb_unlock != NULL) ?
        KII_TRUE : KII_FALSE;
}

static void _lock(kii_session_t* session)
{
    session->_cb_lock(session->_lock_data);
}

static void _unlock(kii_session_t* session)
{
    session->_cb_unlock(session->_lock_data);
}

void kii_session_set_author(kii_session_t* session, const kii_author_t* author)
{
    /* Typically called while restoring a saved author, before the lock is
     * set or any task runs. */
    kii_bool_t locked = _has_lock(session);
    if (locked == KII_TRUE) {
        _lock(session);
    }
    session->_author = *author;
    session->_renew_at_known = KII_FALSE;
    ++session->_generation;
    if (locked == KII_TRUE) {
        _unlock(session);
    }
}

void kii_session_get_author(kii_session_t* session, kii_author_t* out_author)
{
    kii_bool_t locked = _has_lock(session);
    if (locked == KII_TRUE) {
        _lock(session);
    }
    *out_author = session->_author;
    if (locked == KII_TRUE) {
        _unlock(session);
    }
}

static void _sync_locked(kii_t* kii)
{
    kii->_author = kii->_session->_author;
    kii->_session_generation = kii->_session->_generation;
}

void _kii_session_sync(kii_t* kii)
{
    _lock(kii->_session);
    _sync_locked(kii);
    _unlock(kii->_session);
}

kii_code_t kii_set_session(kii_t* kii, kii_session_t* session)
{
    if (session != NULL &&
            (_has_lock(session) != KII_TRUE || session->_cb_monotonic_ms == NULL)) {
        return KII_ERR_FAIL;
    }
    kii->_session = session;
    if (session != NULL) {
        _kii_session_sync(kii);
    }
    return KII_ERR_OK;
}

static uint64_t _now_ms(kii_session_t* session)
{
    return session->_cb_monotonic_ms(session->_monotonic_ms_data);
}

/* How long after it was issued a token with this lifetime is renewed, or
 * KII_FALSE when that is unknown.
 *
 * 0 is a response that gave no lifetime. One that is negative, or longer than
 * _LIFETIME_MAX_SEC, cannot be scheduled either; the token is kept all the
 * same, rather than failing the login or refresh that issued it, and renewed
 * when the server refuses it. Within range the lifetime is at most
 * 4294967295000 ms, far from overflowing. */
static kii_bool_t _renew_after_ms(int64_t expires_in_sec, uint64_t* out_ms)
{
    if (expires_in_sec <= 0 || expires_in_sec > _LIFETIME_MAX_SEC) {
        return KII_FALSE;
    }
    uint64_t lifetime_ms = (uint64_t)expires_in_sec * 1000;
    uint64_t margin_ms = lifetime_ms / 5;
    if (margin_ms > _RENEW_MARGIN_MAX_MS) {
        margin_ms = _RENEW_MARGIN_MAX_MS;
    }
    *out_ms = lifetime_ms - margin_ms;
    return KII_TRUE;
}

static void _store_locked(
        kii_session_t* session,
        const kii_author_t* author,
        int64_t expires_in_sec)
{
    session->_author = *author;
    session->_renew_at_known = _renew_after_ms(expires_in_sec, &session->_renew_after_ms);
    if (session->_renew_at_known == KII_TRUE) {
        session->_stored_at_ms = _now_ms(session);
    }
    ++session->_generation;
    if (session->_cb_author_updated != NULL) {
        session->_cb_author_updated(&session->_author, session->_author_updated_data);
    }
}

void _kii_session_publish(kii_t* kii, int64_t expires_in_sec)
{
    kii_session_t* session = kii->_session;
    _lock(session);
    _store_locked(session, &kii->_author, expires_in_sec);
    _sync_locked(kii);
    _unlock(session);
}

static kii_bool_t _due_locked(kii_session_t* session)
{
    if (session->_renew_at_known != KII_TRUE) {
        return KII_FALSE;
    }
    /* The time elapsed rather than a deadline: the clock's origin is
     * arbitrary, and a deadline added to a large one could wrap around and be
     * due at once. The unsigned difference is right wherever the origin is. */
    uint64_t elapsed_ms = _now_ms(session) - session->_stored_at_ms;
    return (elapsed_ms >= session->_renew_after_ms) ? KII_TRUE : KII_FALSE;
}

kii_code_t _kii_parse_token_response(
        kii_t* kii,
        kii_author_t* author,
        kii_bool_t id_required,
        int64_t* out_expires_in_sec)
{
    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        return KII_ERR_FAIL;
    }

    jkii_field_t fields[5];
    memset(fields, 0, sizeof(fields));
    fields[0].name = "id";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = author->author_id;
    fields[0].field_copy_buff_size = sizeof(author->author_id) /
        sizeof(author->author_id[0]);
    fields[1].name = "access_token";
    fields[1].type = JKII_FIELD_TYPE_STRING;
    fields[1].field_copy.string = author->access_token;
    fields[1].field_copy_buff_size = sizeof(author->access_token) /
        sizeof(author->access_token[0]);
    fields[2].name = "refresh_token";
    fields[2].type = JKII_FIELD_TYPE_STRING;
    fields[2].field_copy.string = author->refresh_token;
    fields[2].field_copy_buff_size = sizeof(author->refresh_token) /
        sizeof(author->refresh_token[0]);
    fields[3].name = "expires_in";
    fields[3].type = JKII_FIELD_TYPE_LONG;
    fields[4].name = NULL;

    jkii_parse_err_t result = _jkii_read_object(kii, buff, buff_size, fields);
    if (result != JKII_ERR_OK && result != JKII_ERR_PARTIAL) {
        return KII_ERR_PARSE_JSON;
    }
    if (fields[1].result != JKII_FIELD_ERR_OK) {
        return KII_ERR_PARSE_JSON;
    }
    if (fields[0].result != JKII_FIELD_ERR_OK &&
            (id_required == KII_TRUE || fields[0].result != JKII_FIELD_ERR_NOT_FOUND)) {
        return KII_ERR_PARSE_JSON;
    }
    /* A refresh token too long to keep is an error, not an absence: the one it
     * replaces is no longer valid. */
    if (fields[2].result != JKII_FIELD_ERR_OK &&
            fields[2].result != JKII_FIELD_ERR_NOT_FOUND) {
        return KII_ERR_PARSE_JSON;
    }
    if (fields[3].result == JKII_FIELD_ERR_OK) {
        *out_expires_in_sec = fields[3].field_copy.long_value;
    } else {
        *out_expires_in_sec = 0;
    }
    return KII_ERR_OK;
}

/* Interprets the outcome of a token request. *rejected is set when the server
 * answered and refused the grant, as opposed to not answering. */
static kii_code_t _token_response(
        kii_t* kii,
        kii_code_t sent,
        kii_author_t* author,
        kii_bool_t id_required,
        int64_t* expires_in_sec,
        kii_bool_t* rejected)
{
    *rejected = KII_FALSE;
    if (sent != KII_ERR_OK) {
        return sent;
    }
    uint16_t status = khc_get_status_code(&kii->_khc);
    if (400 <= status && status < 500) {
        *rejected = KII_TRUE;
        return KII_ERR_RESP_STATUS;
    }
    if (status < 200 || 300 <= status) {
        return KII_ERR_RESP_STATUS;
    }
    return _kii_parse_token_response(kii, author, id_required, expires_in_sec);
}

/* Renews the session's token unless another kii_t did since this one took it,
 * or, when not forced, it is not yet due. Called with the session lock held,
 * and with kii->_rw_buff pointing at space the renewal may use. */
static kii_code_t _renew_locked(kii_t* kii, kii_bool_t force)
{
    kii_session_t* session = kii->_session;
    if (session->_generation != kii->_session_generation) {
        _sync_locked(kii);
        return KII_ERR_OK;
    }
    if (force != KII_TRUE && _due_locked(session) != KII_TRUE) {
        return KII_ERR_OK;
    }

    kii_author_t author = session->_author;
    int64_t expires_in_sec = 0;
    kii_code_t res = KII_ERR_FAIL;
    /* Having no refresh token is treated like having it refused. */
    kii_bool_t rejected = KII_TRUE;
    if (author.refresh_token[0] != '\0') {
        _reset_buff(kii);
        _reset_khc(kii);
        res = _token_response(kii, _thing_refresh(kii, author.refresh_token),
                &author, KII_FALSE, &expires_in_sec, &rejected);
    }
    /* The password stands in for a refresh token the server refused, not for
     * one it could not be asked about: that one may well still be valid. */
    if (res != KII_ERR_OK && rejected == KII_TRUE && session->_password != NULL) {
        author = session->_author;
        /* A login starts a new pair, which has no refresh token unless the
         * server issues one. */
        author.refresh_token[0] = '\0';
        _reset_buff(kii);
        _reset_khc(kii);
        res = _token_response(kii,
                _thing_auth(kii, session->_vendor_thing_id, session->_password),
                &author, KII_TRUE, &expires_in_sec, &rejected);
    }
    if (res == KII_ERR_OK) {
        _store_locked(session, &author, expires_in_sec);
    }
    _sync_locked(kii);
    return res;
}

/* Whether the request being prepared carries the session's token. Looking at
 * the header itself, rather than at which function built it, also covers
 * requests an application builds with kii_api_call_start(). */
static kii_bool_t _has_session_token(kii_t* kii)
{
    if (kii->_author.access_token[0] == '\0') {
        return KII_FALSE;
    }
    size_t prefix_len = strlen(_AUTH_HEADER_PREFIX);
    for (khc_slist* node = kii->_req_headers; node != NULL; node = node->next) {
        if (strncmp(node->data, _AUTH_HEADER_PREFIX, prefix_len) == 0 &&
                strcmp(node->data + prefix_len, kii->_author.access_token) == 0) {
            return KII_TRUE;
        }
    }
    return KII_FALSE;
}

/* Points the request's Authorization header at kii->_author's token. */
static kii_code_t _replace_auth_header(kii_t* kii)
{
    char line[sizeof(_AUTH_HEADER_PREFIX) + KII_TOKEN_MAX_SIZE];
    int len = snprintf(line, sizeof(line), "%s%s",
            _AUTH_HEADER_PREFIX, kii->_author.access_token);
    if (len < 0 || (size_t)len >= sizeof(line)) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_slist* node = kii->_cb_slist_alloc(line, len, kii->_slist_alloc_data);
    if (node == NULL) {
        return KII_ERR_ALLOCATION;
    }
    node->next = NULL;

    const char name[] = "Authorization:";
    khc_slist** link = &kii->_req_headers;
    while (*link != NULL && strncmp((*link)->data, name, strlen(name)) != 0) {
        link = &(*link)->next;
    }
    if (*link != NULL) {
        khc_slist* old = *link;
        node->next = old->next;
        kii->_cb_slist_free(old, kii->_slist_free_data);
    }
    *link = node;
    return KII_ERR_OK;
}

/* Renews the session's token through kii's own khc and buffer, then puts both
 * back as the request under way left them, its Authorization header carrying
 * the token now held.
 *
 * A body built in the buffer survives when the renewal fits in the space after
 * it; *out_resendable tells whether it did. A streamed body never does: its
 * reader belongs to the caller. When not forced, a renewal that would cost the
 * body is not attempted. */
static kii_code_t _renew_and_restore(
        kii_t* kii,
        kii_bool_t force,
        kii_bool_t* out_resendable)
{
    *out_resendable = KII_FALSE;
    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_size;
    size_t req_size = kii->_rw_buff_req_size;
    kii_bool_t streamed = kii->_req_streamed;
    size_t keep = (streamed == KII_TRUE) ? 0 : req_size + 1;
    if (buff_size < keep + _KII_SESSION_RENEW_BUFF_SIZE) {
        if (force != KII_TRUE) {
            return KII_ERR_OK;
        }
        keep = 0;
    }

    /* The renewal is a request of its own through the same khc, which is
     * copied whole to be put back afterwards. */
    khc request = kii->_khc;
    khc_slist* headers = kii->_req_headers;
    kii->_req_headers = NULL;
    kii->_req_streamed = KII_FALSE;
    kii->_rw_buff = buff + keep;
    kii->_rw_buff_size = buff_size - keep;
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);

    _lock(kii->_session);
    kii_code_t res = _renew_locked(kii, force);
    _unlock(kii->_session);

    _req_headers_free_all(kii);
    kii->_khc = request;
    kii->_req_headers = headers;
    kii->_req_streamed = streamed;
    kii->_rw_buff = buff;
    kii->_rw_buff_size = buff_size;
    kii->_rw_buff_req_size = (keep > 0) ? req_size : 0;
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    kii->_resp_offset = 0;
    kii->_etag[0] = '\0';
    kii_code_t header_res = _replace_auth_header(kii);
    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    if (res == KII_ERR_OK) {
        res = header_res;
    }
    if (res == KII_ERR_OK && streamed != KII_TRUE && keep > 0) {
        *out_resendable = KII_TRUE;
    }
    return res;
}

static kii_bool_t _renewal_due(kii_t* kii)
{
    kii_session_t* session = kii->_session;
    _lock(session);
    kii_bool_t due = (session->_generation != kii->_session_generation ||
            _due_locked(session) == KII_TRUE) ? KII_TRUE : KII_FALSE;
    _unlock(session);
    return due;
}

size_t _kii_session_resp_offset(kii_t* kii)
{
    if (kii->_session_request != KII_TRUE || kii->_req_streamed == KII_TRUE ||
            khc_get_status_code(&kii->_khc) != 403) {
        return 0;
    }
    size_t keep = kii->_rw_buff_req_size + 1;
    return (keep < kii->_rw_buff_size) ? keep : 0;
}

void _kii_resp_to_start(kii_t* kii)
{
    size_t offset = kii->_resp_offset;
    if (offset == 0) {
        return;
    }
    memmove(kii->_rw_buff, kii->_rw_buff + offset, kii->_rw_buff_written);
    memset(kii->_rw_buff + kii->_rw_buff_written, '\0',
            kii->_rw_buff_size - kii->_rw_buff_written);
    kii->_resp_offset = 0;
}

/* Whether the response refuses the token it was sent with. Kii answers 401
 * for other reasons, a request without credentials among them. */
static kii_bool_t _token_refused(kii_t* kii)
{
    if (khc_get_status_code(&kii->_khc) != 403) {
        return KII_FALSE;
    }
    char error_code[sizeof(_WRONG_TOKEN)];
    jkii_field_t fields[2];
    memset(fields, 0, sizeof(fields));
    fields[0].name = "errorCode";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = error_code;
    fields[0].field_copy_buff_size = sizeof(error_code);
    fields[1].name = NULL;
    jkii_parse_err_t result = _jkii_read_object(kii,
            kii->_rw_buff + kii->_resp_offset, kii->_rw_buff_written, fields);
    return ((result == JKII_ERR_OK || result == JKII_ERR_PARTIAL) &&
            fields[0].result == JKII_FIELD_ERR_OK &&
            strcmp(error_code, _WRONG_TOKEN) == 0) ? KII_TRUE : KII_FALSE;
}

khc_code _kii_perform(kii_t* kii)
{
    kii->_session_refused = KII_FALSE;
    if (kii->_session == NULL || _has_session_token(kii) != KII_TRUE) {
        return khc_perform(&kii->_khc);
    }

    kii_bool_t resendable = KII_FALSE;
    if (_renewal_due(kii) == KII_TRUE) {
        /* Failing here is not fatal: the token may still be accepted, and a
         * refusal is handled below. */
        _renew_and_restore(kii, KII_FALSE, &resendable);
    }

    kii->_session_request = KII_TRUE;
    khc_code code = khc_perform(&kii->_khc);
    kii->_session_request = KII_FALSE;
    if (code != KHC_ERR_OK || _token_refused(kii) != KII_TRUE) {
        _kii_resp_to_start(kii);
        return code;
    }

    /* The refusal was read past the request's body, unless there was no room
     * for it there. */
    kii_bool_t body_kept = (kii->_resp_offset > 0) ? KII_TRUE : KII_FALSE;
    kii_code_t res = _renew_and_restore(kii, KII_TRUE, &resendable);
    if (res != KII_ERR_OK) {
        return code;
    }
    if (resendable != KII_TRUE || body_kept != KII_TRUE) {
        /* Not sent again, the request leaves the caller its 403, and the next
         * one the token the renewal obtained. */
        kii->_session_refused = KII_TRUE;
        return code;
    }
    khc_reset_except_request(&kii->_khc);
    return khc_perform(&kii->_khc);
}
