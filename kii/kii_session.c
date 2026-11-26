#include <stdlib.h>
#include <string.h>

#include "kii.h"
#include "kii_impl.h"
#include "kii_session_impl.h"
#include "kii_thing_impl.h"
#include "kii_ti_impl.h"
#include <ebisu_mem.h>
#include <ebisu_retry.h>
#include <ebisu_str.h>

/* Built with EBISU_NO_SESSION, as -DEBISU_SESSION=OFF builds it, this file
 * keeps kii_session_t's own functions and the parsing of a token response,
 * which logging in uses too, and leaves out what attaches a session to a kii_t
 * and renews its token. Nothing can then be attached, kii->_session stays
 * NULL, and the functions the rest of kii calls are reduced, at the end, to
 * what they do without a session. */

#ifndef EBISU_NO_SESSION
/* Kii's own SDKs renew five minutes before expiry. Short-lived tokens get a
 * fifth of their lifetime instead, so they are not renewed as soon as issued.
 * A thing renews from that margin to half of it before expiry, its share by
 * its ID, as ebisu_retry_spread() gives it, so that things whose tokens were
 * issued together, as after an outage, don't all renew together. */
#define _RENEW_MARGIN_MAX_MS ((uint64_t)5 * 60 * 1000)

/* Lifetimes beyond this, some 136 years, are treated as unknown rather than
 * scheduled. Kii's default, 2147483647 seconds, is within it. */
#define _LIFETIME_MAX_SEC ((int64_t)UINT32_MAX)

/* After the server refuses the thing's credentials, the session waits at least
 * this long before trying them again, and twice as long after each further
 * refusal, up to _PASSWORD_WAIT_MAX_MS. Where the application limits login
 * attempts, refused logins lock the thing out, and a wrong password would
 * otherwise be tried on every request refused for its token; spread out like
 * this, few of them fall within the period the application counts them over.
 * Not giving up altogether lets the thing log in again unattended once its
 * password is put right on the server.
 *
 * ebisu_retry.h's waits, from twice the one up to twice the other, so that a
 * thing waits from one to two times these, its share from its vendor thing ID:
 * things the server refused together don't try again together. */
#define _PASSWORD_WAIT_MIN_MS ((uint32_t)5 * 60 * 1000)
#define _PASSWORD_WAIT_MAX_MS ((uint32_t)60 * 60 * 1000)

/* A renewal ahead of expiry that fails, as when the server can't be reached,
 * is tried again after ebisu_retry.h's waits, from this up to
 * _RENEW_RETRY_MAX_MS, each thing its share by its ID, rather than before
 * every request: the token is still good meanwhile, and a request that first
 * renewed would be two that fail while the server is down. A token the server
 * refuses is renewed at once all the same. */
#define _RENEW_RETRY_MIN_MS ((uint32_t)30 * 1000)
#define _RENEW_RETRY_MAX_MS ((uint32_t)5 * 60 * 1000)

/* What kii.h tells applications a renewal takes, either way. */
#if KII_SESSION_RENEW_BUFF_SIZE < _KII_SESSION_RENEW_BUFF_SIZE || \
    KII_SESSION_RENEW_BUFF_SIZE < _KII_SESSION_ONBOARD_BUFF_SIZE
#error "KII_SESSION_RENEW_BUFF_SIZE must hold either renewal"
#endif

/* What a renewal allocates its room with: malloc(), unless the tests build kii
 * with an allocator of their own, which can fail. */
#ifdef _KII_RENEW_MALLOC
void* _KII_RENEW_MALLOC(size_t size);
#else
#define _KII_RENEW_MALLOC malloc
#endif

static const char _AUTH_HEADER_PREFIX[] = "Authorization: Bearer ";

/* The errorCode of the 403 Kii answers for a token that is not valid: one that
 * has expired, or been replaced by a renewal. */
static const char _WRONG_TOKEN[] = "WRONG_TOKEN";
#endif /* EBISU_NO_SESSION */

void kii_session_init(kii_session_t* session)
{
    memset(session, 0x00, sizeof(kii_session_t));
    session->_renew_at_known = false;
}

void kii_session_set_lock(kii_session_t* session, ebisu_lock_t* lock)
{
    session->_lock = lock;
}

void kii_session_set_system(kii_session_t* session, ebisu_system_t* system)
{
    session->_system = system;
}

void kii_session_set_renew_buff(kii_session_t* session, char* buff, size_t buff_size)
{
    session->_renew_buff = buff;
    session->_renew_buff_size = (buff != NULL) ? buff_size : 0;
}

void kii_session_set_cb_author_updated(
        kii_session_t* session,
        KII_CB_AUTHOR_UPDATED cb_author_updated,
        void* userdata)
{
    session->_cb_author_updated = cb_author_updated;
    session->_author_updated_data = userdata;
}

static bool _has_lock(kii_session_t* session)
{
    return session->_lock != NULL;
}

static void _lock(kii_session_t* session)
{
    session->_lock->ops->lock(session->_lock);
}

static void _unlock(kii_session_t* session)
{
    session->_lock->ops->unlock(session->_lock);
}

void kii_session_set_password(
        kii_session_t* session,
        const char* vendor_thing_id,
        const char* password)
{
    /* Typically called before the lock is set, but also to put right
     * credentials the server refused, while tasks run. */
    bool locked = _has_lock(session);
    if (locked) {
        _lock(session);
    }
    session->_vendor_thing_id = vendor_thing_id;
    session->_password = password;
    session->_password_wait_ms = 0;
    session->_password_refusals = 0;
    if (locked) {
        _unlock(session);
    }
}

void kii_session_set_author(kii_session_t* session, const kii_author_t* author)
{
    /* Typically called while restoring a saved author, before the lock is
     * set or any task runs. */
    bool locked = _has_lock(session);
    if (locked) {
        _lock(session);
    }
    session->_author = *author;
    session->_renew_at_known = false;
    ++session->_generation;
    if (locked) {
        _unlock(session);
    }
}

void kii_session_set_renew_mode(kii_session_t* session, kii_renew_mode_t mode)
{
    if (mode != KII_RENEW_BY_TOKEN_ENDPOINT && mode != KII_RENEW_BY_ONBOARDING) {
        return;
    }
    bool locked = _has_lock(session);
    if (locked) {
        _lock(session);
    }
    session->_renew_mode = mode;
    if (locked) {
        _unlock(session);
    }
}

void kii_session_get_author(kii_session_t* session, kii_author_t* out_author)
{
    bool locked = _has_lock(session);
    if (locked) {
        _lock(session);
    }
    *out_author = session->_author;
    if (locked) {
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
#ifdef EBISU_NO_SESSION
    if (session != NULL) {
        return KII_ERR_FAIL;
    }
#else
    if (session != NULL &&
            (!_has_lock(session) || session->_system == NULL ||
             session->_system->ops->monotonic_ms == NULL)) {
        return KII_ERR_FAIL;
    }
#endif
    kii->_session = session;
    if (session != NULL) {
        _kii_session_sync(kii);
    }
    return KII_ERR_OK;
}

#ifndef EBISU_NO_SESSION
static uint64_t _now_ms(kii_session_t* session)
{
    return session->_system->ops->monotonic_ms(session->_system);
}

/* How long after it was issued a token with this lifetime is renewed, by the
 * thing whose ID gave the seed, or false when that is unknown.
 *
 * 0 is a response that gave no lifetime. One that is negative, or longer than
 * _LIFETIME_MAX_SEC, cannot be scheduled either; the token is kept all the
 * same, rather than failing the login or refresh that issued it, and renewed
 * when the server refuses it. Within range the lifetime is at most
 * 4294967295000 ms, far from overflowing. */
static bool _renew_after_ms(int64_t expires_in_sec, uint32_t seed, uint64_t* out_ms)
{
    if (expires_in_sec <= 0 || expires_in_sec > _LIFETIME_MAX_SEC) {
        return false;
    }
    uint64_t lifetime_ms = (uint64_t)expires_in_sec * 1000;
    uint64_t margin_ms = lifetime_ms / 5;
    if (margin_ms > _RENEW_MARGIN_MAX_MS) {
        margin_ms = _RENEW_MARGIN_MAX_MS;
    }
    /* At most _RENEW_MARGIN_MAX_MS, which a uint32_t holds. */
    *out_ms = lifetime_ms - ebisu_retry_spread((uint32_t)margin_ms, seed);
    return true;
}

static void _store_locked(
        kii_session_t* session,
        const kii_author_t* author,
        int64_t expires_in_sec)
{
    session->_author = *author;
    session->_renew_at_known = _renew_after_ms(expires_in_sec,
            ebisu_retry_seed(author->author_id), &session->_renew_after_ms);
    session->_renew_failures = 0;
    if (session->_renew_at_known) {
        session->_stored_at_ms = _now_ms(session);
    }
    ++session->_generation;
    if (session->_cb_author_updated != NULL) {
        session->_cb_author_updated(&session->_author, session->_author_updated_data);
    }
}

void _kii_session_publish(kii_t* kii, int64_t expires_in_sec, kii_renew_mode_t mode)
{
    kii_session_t* session = kii->_session;
    _lock(session);
    session->_renew_mode = mode;
    _store_locked(session, &kii->_author, expires_in_sec);
    _sync_locked(kii);
    _unlock(session);
}

/* Whether the session may try the thing's credentials: it has them, and is not
 * waiting after the server refused them. */
static bool _password_usable_locked(kii_session_t* session)
{
    if (session->_password == NULL) {
        return false;
    }
    if (session->_password_wait_ms == 0) {
        return true;
    }
    /* As in _due_locked(), the time elapsed rather than a deadline. */
    uint64_t elapsed_ms = _now_ms(session) - session->_password_refused_at_ms;
    return (elapsed_ms >= session->_password_wait_ms);
}

/* Whether a status from a login or onboarding refuses the credentials. 408 and
 * 429 say the server wouldn't look at them yet, which says nothing about them. */
static bool _status_refuses(uint16_t status)
{
    return (400 <= status && status < 500 && status != 408 && status != 429);
}

/* Records the outcome of a login or onboarding with the thing's credentials.
 * An answer that refuses them starts a wait, longer than the last; one that
 * accepts them ends it. When the server could not be asked, or failed, nothing
 * is known about them. */
static void _password_tried_locked(kii_session_t* session, kii_code_t res, bool refused)
{
    if (res == KII_ERR_OK) {
        session->_password_wait_ms = 0;
        session->_password_refusals = 0;
    } else if (refused) {
        ++session->_password_refusals;
        session->_password_wait_ms = ebisu_retry_wait(2 * _PASSWORD_WAIT_MIN_MS,
                2 * _PASSWORD_WAIT_MAX_MS, session->_password_refusals,
                ebisu_retry_seed(session->_vendor_thing_id));
        session->_password_refused_at_ms = _now_ms(session);
    }
}

static bool _due_locked(kii_session_t* session)
{
    if (!session->_renew_at_known) {
        return false;
    }
    /* The time elapsed rather than a deadline: the clock's origin is
     * arbitrary, and a deadline added to a large one could wrap around and be
     * due at once. The unsigned difference is right wherever the origin is. */
    uint64_t now_ms = _now_ms(session);
    if (now_ms - session->_stored_at_ms < session->_renew_after_ms) {
        return false;
    }
    if (session->_renew_failures == 0) {
        return true;
    }
    uint32_t wait_ms = ebisu_retry_wait(_RENEW_RETRY_MIN_MS, _RENEW_RETRY_MAX_MS,
            session->_renew_failures, ebisu_retry_seed(session->_author.author_id));
    return (now_ms - session->_renew_failed_at_ms >= wait_ms);
}

/* Records a renewal ahead of expiry that failed, which the next waits for. */
static void _renew_failed_locked(kii_session_t* session)
{
    ++session->_renew_failures;
    session->_renew_failed_at_ms = _now_ms(session);
}
#endif /* EBISU_NO_SESSION */

kii_code_t _kii_check_token_response(kii_t* kii, bool id_required)
{
    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        return KII_ERR_FAIL;
    }

    /* As _kii_parse_token_response() reads it, with nothing copied. */
    jkii_field_t fields[4];
    memset(fields, 0, sizeof(fields));
    fields[0].name = "id";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[1].name = "access_token";
    fields[1].type = JKII_FIELD_TYPE_STRING;
    fields[2].name = "refresh_token";
    fields[2].type = JKII_FIELD_TYPE_STRING;
    fields[3].name = NULL;

    jkii_parse_err_t result = jkii_parse(buff, buff_size, fields);
    if (result != JKII_ERR_OK && result != JKII_ERR_PARTIAL) {
        return KII_ERR_PARSE_JSON;
    }
    if (!_kii_field_fits(kii, &fields[1], KII_TOKEN_MAX_SIZE, "access token",
                "KII_TOKEN_MAX_SIZE") ||
            !_kii_field_fits(kii, &fields[2], KII_REFRESH_TOKEN_MAX_SIZE, "refresh token",
                "KII_REFRESH_TOKEN_MAX_SIZE")) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    if (fields[1].result != JKII_FIELD_ERR_OK) {
        return KII_ERR_PARSE_JSON;
    }
    if (fields[0].result != JKII_FIELD_ERR_OK &&
            (id_required || fields[0].result != JKII_FIELD_ERR_NOT_FOUND)) {
        return KII_ERR_PARSE_JSON;
    }
    if (fields[0].result == JKII_FIELD_ERR_OK &&
            fields[0].end - fields[0].start >= KII_AUTHOR_ID_MAX_SIZE) {
        return KII_ERR_PARSE_JSON;
    }
    if (fields[2].result != JKII_FIELD_ERR_OK &&
            fields[2].result != JKII_FIELD_ERR_NOT_FOUND) {
        return KII_ERR_PARSE_JSON;
    }
    return KII_ERR_OK;
}

kii_code_t _kii_parse_token_response(
        kii_t* kii,
        kii_author_t* author,
        bool id_required,
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

    jkii_parse_err_t result = jkii_parse(buff, buff_size, fields);
    if (result != JKII_ERR_OK && result != JKII_ERR_PARTIAL) {
        return KII_ERR_PARSE_JSON;
    }
    if (_kii_field_too_long(kii, &fields[1], "access token", "KII_TOKEN_MAX_SIZE") ||
            _kii_field_too_long(kii, &fields[2], "refresh token",
                "KII_REFRESH_TOKEN_MAX_SIZE")) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    if (fields[1].result != JKII_FIELD_ERR_OK) {
        return KII_ERR_PARSE_JSON;
    }
    if (fields[0].result != JKII_FIELD_ERR_OK &&
            (id_required || fields[0].result != JKII_FIELD_ERR_NOT_FOUND)) {
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

#ifndef EBISU_NO_SESSION
/* Interprets the outcome of a token request. *rejected is set when the server
 * answered and refused the grant, as opposed to not answering. */
static kii_code_t _token_response(
        kii_t* kii,
        kii_code_t sent,
        kii_author_t* author,
        bool id_required,
        int64_t* expires_in_sec,
        bool* rejected)
{
    *rejected = false;
    kii_code_t res = sent;
    if (res == KII_ERR_OK) {
        uint16_t status = khc_get_status_code(&kii->_khc);
        if (_status_refuses(status)) {
            *rejected = true;
            res = KII_ERR_RESP_STATUS;
        } else if (status < 200 || 300 <= status) {
            res = KII_ERR_RESP_STATUS;
        } else {
            res = _kii_parse_token_response(kii, author, id_required, expires_in_sec);
        }
    }
    /* The request carried the refresh token or the password, and its response
     * the tokens. */
    _kii_clear_buffers(kii);
    return res;
}

/* Renews a token onboarding obtained by onboarding again, which issues a new
 * one, with neither a lifetime nor a refresh token. Called with the session
 * lock held. */
static kii_code_t _onboard_again(kii_t* kii, kii_author_t* author)
{
    kii_session_t* session = kii->_session;
    if (!_password_usable_locked(session)) {
        return KII_ERR_FAIL;
    }
    kii_code_t res = _onboard(kii, session->_vendor_thing_id,
            session->_password, NULL, NULL, NULL, NULL);
    uint16_t status = khc_get_status_code(&kii->_khc);
    _password_tried_locked(session, res,
            res == KII_ERR_RESP_STATUS && _status_refuses(status));
    if (res != KII_ERR_OK) {
        return res;
    }
    /* _onboard() wrote the new token and the thing ID to kii->_author. */
    *author = kii->_author;
    ebisu_secure_zero(author->refresh_token, sizeof(author->refresh_token));
    return KII_ERR_OK;
}

bool _kii_session_renews_by_onboarding(kii_t* kii)
{
    kii_session_t* session = kii->_session;
    if (session == NULL) {
        return false;
    }
    _lock(session);
    bool by_onboarding = (session->_renew_mode == KII_RENEW_BY_ONBOARDING);
    _unlock(session);
    return by_onboarding;
}

kii_code_t _kii_session_onboard_again(kii_t* kii)
{
    kii_session_t* session = kii->_session;
    _lock(session);
    kii_author_t author = session->_author;
    kii_code_t res = _onboard_again(kii, &author);
    if (res == KII_ERR_OK) {
        _store_locked(session, &author, 0);
    }
    ebisu_secure_zero(&author, sizeof(author));
    _sync_locked(kii);
    _unlock(session);
    return res;
}

/* Renews the session's token unless another kii_t did since this one took it,
 * or, when not forced, it is not yet due. Called with the session lock held,
 * and with kii->_rw_buff pointing at space the renewal may use. */
static kii_code_t _renew_locked(kii_t* kii, bool force)
{
    kii_session_t* session = kii->_session;
    if (session->_generation != kii->_session_generation) {
        _sync_locked(kii);
        return KII_ERR_OK;
    }
    if (!force && !_due_locked(session)) {
        return KII_ERR_OK;
    }

    /* A copy of the tokens, cleared before returning. */
    kii_author_t author = session->_author;
    int64_t expires_in_sec = 0;
    kii_code_t res = KII_ERR_FAIL;
    if (session->_renew_mode == KII_RENEW_BY_ONBOARDING) {
        res = _onboard_again(kii, &author);
        if (res == KII_ERR_OK) {
            _store_locked(session, &author, 0);
        } else if (!force) {
            _renew_failed_locked(session);
        }
        ebisu_secure_zero(&author, sizeof(author));
        _sync_locked(kii);
        return res;
    }
    /* Having no refresh token is treated like having it refused. */
    bool rejected = true;
    if (author.refresh_token[0] != '\0') {
        _reset_buff(kii);
        _reset_khc(kii);
        res = _token_response(kii, _thing_refresh(kii, author.refresh_token),
                &author, false, &expires_in_sec, &rejected);
    }
    /* The password stands in for a refresh token the server refused, not for
     * one it could not be asked about: that one may well still be valid. */
    if (res != KII_ERR_OK && rejected &&
            _password_usable_locked(session)) {
        author = session->_author;
        /* A login starts a new pair, which has no refresh token unless the
         * server issues one. */
        ebisu_secure_zero(author.refresh_token, sizeof(author.refresh_token));
        _reset_buff(kii);
        _reset_khc(kii);
        res = _token_response(kii,
                _thing_auth(kii, session->_vendor_thing_id, session->_password),
                &author, true, &expires_in_sec, &rejected);
        _password_tried_locked(session, res, rejected);
    }
    if (res == KII_ERR_OK) {
        _store_locked(session, &author, expires_in_sec);
    } else if (!force) {
        _renew_failed_locked(session);
    }
    ebisu_secure_zero(&author, sizeof(author));
    _sync_locked(kii);
    return res;
}

/* Where the request's Authorization header starts, the first, and its length
 * with its CRLF: 0, and the end of the headers, when it has none. kii's
 * headers are lines with a CRLF each, and no CR before it. */
static size_t _find_auth_header(const kii_t* kii, size_t* out_len)
{
    static const char name[] = "Authorization:";
    for (size_t pos = 0; pos < kii->_req_headers_len;) {
        const char* line = &kii->_req_headers[pos];
        const char* cr = memchr(line, '\r', kii->_req_headers_len - pos);
        size_t line_len = (cr != NULL) ? (size_t)(cr - line) + 2 : kii->_req_headers_len - pos;
        if (line_len >= sizeof(name) - 1 && memcmp(line, name, sizeof(name) - 1) == 0) {
            *out_len = line_len;
            return pos;
        }
        pos += line_len;
    }
    *out_len = 0;
    return kii->_req_headers_len;
}

/* Whether the request being prepared carries the session's token. Looking at
 * the header itself, rather than at which function built it, also covers
 * requests an application builds with kii_api_call_start(). */
static bool _has_session_token(kii_t* kii)
{
    if (kii->_author.access_token[0] == '\0') {
        return false;
    }
    size_t line_len;
    size_t at = _find_auth_header(kii, &line_len);
    size_t prefix_len = strlen(_AUTH_HEADER_PREFIX);
    size_t token_len = strlen(kii->_author.access_token);
    return line_len == prefix_len + token_len + 2 &&
        memcmp(&kii->_req_headers[at], _AUTH_HEADER_PREFIX, prefix_len) == 0 &&
        memcmp(&kii->_req_headers[at + prefix_len], kii->_author.access_token, token_len) == 0;
}

/* Points the request's Authorization header at kii->_author's token, in place
 * of the one it had, or after its other headers. */
static kii_code_t _replace_auth_header(kii_t* kii)
{
    kii_code_t res = _req_headers_open(kii);
    if (res != KII_ERR_OK) {
        return res;
    }
    size_t old_len;
    size_t at = _find_auth_header(kii, &old_len);
    size_t prefix_len = strlen(_AUTH_HEADER_PREFIX);
    size_t token_len = strlen(kii->_author.access_token);
    size_t new_len = prefix_len + token_len + 2;
    size_t len = kii->_req_headers_len - old_len + new_len;
    if (len > kii->_req_headers_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    char* headers = kii->_req_headers;
    /* The headers after it move to where the new line ends. What a shorter
     * line leaves of the old token after them isn't sent, and is cleared with
     * the rest once the request has been. */
    memmove(&headers[at + new_len], &headers[at + old_len],
            kii->_req_headers_len - at - old_len);
    memcpy(&headers[at], _AUTH_HEADER_PREFIX, prefix_len);
    memcpy(&headers[at + prefix_len], kii->_author.access_token, token_len);
    memcpy(&headers[at + prefix_len + token_len], "\r\n", 2);
    kii->_req_headers_len = len;
    return KII_ERR_OK;
}

/* Renews the session's token through kii's own khc and buffer, then puts both
 * back as the request under way left them, its Authorization header carrying
 * the token now held.
 *
 * A body built in the buffer is kept: the renewal takes the room after it, or,
 * where that is too little, the session's buffer, or one allocated for it and
 * freed. Without any, the body would be lost: KII_ERR_ALLOCATION, nothing
 * renewed, or, when not forced, KII_ERR_OK, nothing attempted. A streamed body,
 * whose reader belongs to the caller, and an empty one have nothing to keep:
 * without room of its own, the renewal takes what the buffer has, or, when not
 * forced, isn't attempted. *out_resendable tells whether the request can be
 * sent again. A token another kii_t of the session renewed is only taken,
 * which asks nothing of the server and needs no room. */
/* What kii_get_etag() gives, put back after a response that isn't the
 * application's: a renewal's, or a refusal's once the request is sent again.
 * The next response the application gets replaces it, and a request that
 * gets none leaves it, as without a session. */
typedef struct {
    char etag[KII_ETAG_MAX_SIZE];
    bool lost;
} _etag_copy_t;

static void _etag_copy(const kii_t* kii, _etag_copy_t* copy)
{
    memcpy(copy->etag, kii->_etag, sizeof(copy->etag));
    copy->lost = kii->_etag_lost;
}

static void _etag_put_back(kii_t* kii, const _etag_copy_t* copy)
{
    memcpy(kii->_etag, copy->etag, sizeof(kii->_etag));
    kii->_etag_lost = copy->lost;
}

static kii_code_t _renew_and_restore(
        kii_t* kii,
        bool force,
        bool* out_resendable)
{
    *out_resendable = false;
    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_size;
    size_t req_size = kii->_rw_buff_req_size;
    bool streamed = kii->_req_streamed;
    /* The body, and the byte after it, which kii_api_call_append_body() always
     * leaves. */
    size_t keep = (streamed || req_size == 0) ? 0 : req_size + 1;
    _lock(kii->_session);
    const bool renewed_elsewhere =
        kii->_session->_generation != kii->_session_generation;
    size_t renewal_size = (kii->_session->_renew_mode == KII_RENEW_BY_ONBOARDING) ?
        _KII_SESSION_ONBOARD_BUFF_SIZE : _KII_SESSION_RENEW_BUFF_SIZE;
    char* session_room = kii->_session->_renew_buff;
    size_t session_room_size = kii->_session->_renew_buff_size;
    _unlock(kii->_session);

    char* room = buff + keep;
    size_t room_size = buff_size - keep;
    char* allocated = NULL;
    /* Once renewed elsewhere, the generation never comes back to this kii_t's
     * until it takes it, so _renew_locked() takes it below. */
    if (!renewed_elsewhere && room_size < renewal_size) {
        if (session_room != NULL) {
            room = session_room;
            room_size = session_room_size;
        } else {
            allocated = (char*)_KII_RENEW_MALLOC(renewal_size);
            if (allocated != NULL) {
                room = allocated;
                room_size = renewal_size;
            } else if (keep > 0) {
                return (force) ? KII_ERR_ALLOCATION : KII_ERR_OK;
            } else if (!force) {
                return KII_ERR_OK;
            }
        }
    }

    /* The renewal is a request of its own through the same khc, which is
     * copied whole to be put back afterwards. Its headers go after the
     * request's, in the room left in the same buffer, or, without one yet, in
     * one of their own. */
    khc_t request = kii->_khc;
    _etag_copy_t etag;
    _etag_copy(kii, &etag);
    char* header_buff = kii->_req_header_buff;
    size_t header_buff_size = kii->_req_header_buff_size;
    char* headers = kii->_req_headers;
    size_t headers_size = kii->_req_headers_size;
    size_t headers_len = kii->_req_headers_len;
    bool headers_allocated = kii->_req_headers_allocated;
    if (headers != NULL) {
        kii->_req_header_buff = headers + headers_len;
        kii->_req_header_buff_size = headers_size - headers_len;
    }
    kii->_req_headers = NULL;
    kii->_req_headers_size = 0;
    kii->_req_headers_len = 0;
    kii->_req_headers_allocated = false;
    kii->_req_streamed = false;
    kii->_rw_buff = room;
    kii->_rw_buff_size = room_size;
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);

    _lock(kii->_session);
    /* The renewal clears what it leaves of the tokens and credentials in the
     * room it took, as it does in kii's own buffer, before another kii_t can
     * take the session's. */
    kii_code_t res = _renew_locked(kii, force);
    _unlock(kii->_session);
    free(allocated);

    _req_headers_free_all(kii);
    kii->_khc = request;
    kii->_req_header_buff = header_buff;
    kii->_req_header_buff_size = header_buff_size;
    kii->_req_headers = headers;
    kii->_req_headers_size = headers_size;
    kii->_req_headers_len = headers_len;
    kii->_req_headers_allocated = headers_allocated;
    kii->_req_streamed = streamed;
    kii->_rw_buff = buff;
    kii->_rw_buff_size = buff_size;
    kii->_rw_buff_req_size = (streamed) ? 0 : req_size;
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    kii->_resp_offset = 0;
    _kii_clear_resp_headers_but_etag(kii);
    _etag_put_back(kii, &etag);
    kii_code_t header_res = _replace_auth_header(kii);
    khc_set_req_headers(&kii->_khc, kii->_req_headers, kii->_req_headers_len);
    if (res == KII_ERR_OK) {
        res = header_res;
    }
    if (res == KII_ERR_OK && !streamed) {
        *out_resendable = true;
    }
    return res;
}

static bool _renewal_due(kii_t* kii)
{
    kii_session_t* session = kii->_session;
    _lock(session);
    bool due = (session->_generation != kii->_session_generation ||
            _due_locked(session));
    _unlock(session);
    return due;
}

size_t _kii_session_resp_offset(kii_t* kii)
{
    if (!kii->_session_request || kii->_req_streamed ||
            khc_get_status_code(&kii->_khc) != 403) {
        return 0;
    }
    size_t keep = kii->_rw_buff_req_size + 1;
    return (keep < kii->_rw_buff_size) ? keep : 0;
}
#endif /* EBISU_NO_SESSION */

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

#ifndef EBISU_NO_SESSION
/* Whether the response refuses the token it was sent with. Kii answers 401
 * for other reasons, a request without credentials among them. */
static bool _token_refused(kii_t* kii)
{
    if (khc_get_status_code(&kii->_khc) != 403) {
        return false;
    }
    char error_code[sizeof(_WRONG_TOKEN)];
    jkii_field_t fields[2];
    memset(fields, 0, sizeof(fields));
    fields[0].name = "errorCode";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = error_code;
    fields[0].field_copy_buff_size = sizeof(error_code);
    fields[1].name = NULL;
    jkii_parse_err_t result = jkii_parse(kii->_rw_buff + kii->_resp_offset,
            kii->_rw_buff_written, fields);
    return ((result == JKII_ERR_OK || result == JKII_ERR_PARTIAL) &&
            fields[0].result == JKII_FIELD_ERR_OK &&
            strcmp(error_code, _WRONG_TOKEN) == 0);
}

khc_code_t _kii_perform(kii_t* kii)
{
    kii->_session_refused = false;
    if (kii->_session == NULL || !_has_session_token(kii)) {
        return khc_perform(&kii->_khc);
    }

    /* The last response's, for the request to be sent again with. */
    _etag_copy_t etag;
    _etag_copy(kii, &etag);
    bool resendable = false;
    if (_renewal_due(kii)) {
        /* Failing here is not fatal: the token may still be accepted, and a
         * refusal is handled below. */
        _renew_and_restore(kii, false, &resendable);
    }

    kii->_session_request = true;
    khc_code_t code = khc_perform(&kii->_khc);
    kii->_session_request = false;
    if (code != KHC_ERR_OK || !_token_refused(kii)) {
        _kii_resp_to_start(kii);
        return code;
    }

    /* The refusal was read past the request's body, unless there was no room
     * for it there. */
    bool body_kept = (kii->_resp_offset > 0);
    kii_code_t res = _renew_and_restore(kii, true, &resendable);
    if (res == KII_ERR_ALLOCATION) {
        /* No room for the renewal without losing the body: the request fails,
         * the token not renewed, and the refusal stays the response. */
        _kii_resp_to_start(kii);
        return KHC_ERR_ALLOCATION;
    }
    if (res != KII_ERR_OK) {
        return code;
    }
    if (!resendable || !body_kept) {
        /* Not sent again, the request leaves the caller its 403, and the next
         * one the token the renewal obtained. */
        kii->_session_refused = true;
        return code;
    }
    /* The refusal isn't the response the application gets. */
    _etag_put_back(kii, &etag);
    khc_reset_except_request(&kii->_khc);
    return khc_perform(&kii->_khc);
}

#else /* EBISU_NO_SESSION */

/* What the rest of kii calls, on every request or past a check for a session,
 * as it is without one. */

void _kii_session_publish(kii_t* kii, int64_t expires_in_sec, kii_renew_mode_t mode)
{
    (void)kii;
    (void)expires_in_sec;
    (void)mode;
}

bool _kii_session_renews_by_onboarding(kii_t* kii)
{
    (void)kii;
    return false;
}

kii_code_t _kii_session_onboard_again(kii_t* kii)
{
    (void)kii;
    return KII_ERR_FAIL;
}

size_t _kii_session_resp_offset(kii_t* kii)
{
    (void)kii;
    return 0;
}

khc_code_t _kii_perform(kii_t* kii)
{
    kii->_session_refused = false;
    return khc_perform(&kii->_khc);
}

#endif /* EBISU_NO_SESSION */
