#ifndef __kii_session_impl__
#define __kii_session_impl__

#include "kii.h"
#include "khc.h"

/* Space a renewal needs in kii_t::_rw_buff for its request and response: the
 * response carries the thing ID, both tokens and a few short fields. */
#define _KII_SESSION_RENEW_BUFF_SIZE \
    (KII_AUTHOR_ID_MAX_SIZE + KII_TOKEN_MAX_SIZE + KII_REFRESH_TOKEN_MAX_SIZE + 256)

/* Space a renewal by onboarding again needs: its response also carries the
 * MQTT endpoint, where MQTT is enabled. */
#define _KII_SESSION_ONBOARD_BUFF_SIZE \
    (KII_AUTHOR_ID_MAX_SIZE + KII_TOKEN_MAX_SIZE + KII_INSTALLATION_ID_MAX_SIZE + \
     KII_USERNAME_MAX_SIZE + KII_PASSWORD_MAX_SIZE + KII_TOPIC_MAX_SIZE + \
     KII_HOST_MAX_SIZE + 512)

/* Copies the session's author into kii->_author. */
void _kii_session_sync(kii_t* kii);

/* Stores the tokens a login or onboarding just wrote to kii->_author in the
 * session, to be renewed the same way: by onboarding again when
 * by_onboarding, and otherwise through the token endpoint. expires_in_sec is
 * the token's lifetime as the response gave it, or 0 when it gave none. One
 * that is not positive or exceeds UINT32_MAX seconds leaves the expiry
 * unknown, as 0 does. */
void _kii_session_publish(kii_t* kii, int64_t expires_in_sec, kii_bool_t by_onboarding);

/* khc_perform() for requests that may carry the session's token: renews the
 * token close to expiry or when the server refuses it, and then sends the
 * request again when its body allows. Without a session it is khc_perform(). */
khc_code _kii_perform(kii_t* kii);

/* Where in kii->_rw_buff the response body being received starts: past the
 * request's body when the response is a 403 to a request carrying the
 * session's token, which may refuse the token and have the request sent
 * again, and otherwise 0. */
size_t _kii_session_resp_offset(kii_t* kii);

/* Moves the response body read so far to the start of kii->_rw_buff, where
 * callers find it. */
void _kii_resp_to_start(kii_t* kii);

/* Parses an OAuth2 token response in kii->_rw_buff into author.
 *
 * "access_token" is required, and "id" when id_required. "refresh_token" is
 * copied when present and otherwise left as it is. *out_expires_in_sec is set
 * from "expires_in" as it is, or to 0 when absent or not an integer that fits
 * an int64_t. */
kii_code_t _kii_parse_token_response(
        kii_t* kii,
        kii_author_t* author,
        kii_bool_t id_required,
        int64_t* out_expires_in_sec);

#endif
