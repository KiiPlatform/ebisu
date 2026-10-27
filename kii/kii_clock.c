#include "kii.h"
#include "kii_impl.h"
#include "ebisu_version.h"

#include <string.h>

static void _lock_clock(kii_clock_t* clock)
{
    if (clock->_cb_lock != NULL) {
        clock->_cb_lock(clock->_lock_data);
    }
}

static void _unlock_clock(kii_clock_t* clock)
{
    if (clock->_cb_unlock != NULL) {
        clock->_cb_unlock(clock->_lock_data);
    }
}

void kii_clock_init(
        kii_clock_t* clock,
        KII_CB_MONOTONIC_MS cb_monotonic_ms,
        void* userdata)
{
    memset(clock, 0, sizeof(*clock));
    /* The floor khc_init() sets khc: a week's margin, for a committer whose
     * clock was a little ahead. */
    clock->_floor = EBISU_SOURCE_TIME - 7 * 86400;
    clock->_time_known = KII_FALSE;
    clock->_cb_monotonic_ms = cb_monotonic_ms;
    clock->_monotonic_ms_data = userdata;
}

void kii_clock_set_cb_lock(
        kii_clock_t* clock,
        KII_CB_LOCK cb_lock,
        KII_CB_LOCK cb_unlock,
        void* userdata)
{
    clock->_cb_lock = cb_lock;
    clock->_cb_unlock = cb_unlock;
    clock->_lock_data = userdata;
}

void kii_clock_set_floor(kii_clock_t* clock, int64_t floor)
{
    _lock_clock(clock);
    if (floor > clock->_floor) {
        clock->_floor = floor;
    }
    _unlock_clock(clock);
}

kii_bool_t kii_clock_set_time(kii_clock_t* clock, int64_t now)
{
    kii_bool_t taken = KII_FALSE;
    _lock_clock(clock);
    if (now >= clock->_floor && clock->_cb_monotonic_ms != NULL) {
        clock->_time = now;
        clock->_time_at_ms = clock->_cb_monotonic_ms(clock->_monotonic_ms_data);
        clock->_time_known = KII_TRUE;
        taken = KII_TRUE;
    }
    _unlock_clock(clock);
    return taken;
}

kii_bool_t kii_clock_get_time(kii_clock_t* clock, int64_t* out_now)
{
    kii_bool_t exact = KII_FALSE;
    _lock_clock(clock);
    int64_t now = clock->_floor;
    if (clock->_time_known == KII_TRUE && clock->_cb_monotonic_ms != NULL) {
        uint64_t now_ms = clock->_cb_monotonic_ms(clock->_monotonic_ms_data);
        /* A monotonic clock that went back counts as no time elapsed, rather
         * than as most of 2^64 milliseconds. */
        uint64_t elapsed_ms = (now_ms >= clock->_time_at_ms) ? now_ms - clock->_time_at_ms : 0;
        int64_t counted = clock->_time + (int64_t)(elapsed_ms / 1000);
        /* A floor set later than the time counted on is the better bound. */
        if (counted >= now) {
            now = counted;
            exact = KII_TRUE;
        }
    }
    _unlock_clock(clock);
    *out_now = now;
    return exact;
}

kii_validity_t kii_clock_check_validity(
        kii_clock_t* clock,
        int64_t not_before,
        int64_t not_after)
{
    int64_t now = 0;
    kii_bool_t exact = kii_clock_get_time(clock, &now);
    if (now > not_after) {
        return KII_VALIDITY_EXPIRED;
    }
    if (exact == KII_TRUE && now < not_before) {
        return KII_VALIDITY_NOT_YET_VALID;
    }
    return KII_VALIDITY_OK;
}

kii_code_t kii_set_clock(kii_t* kii, kii_clock_t* clock)
{
    if (clock != NULL && clock->_cb_monotonic_ms == NULL) {
        return KII_ERR_FAIL;
    }
    kii->_clock = clock;
    return KII_ERR_OK;
}

void _kii_clock_record(kii_t* kii)
{
    if (kii->_clock == NULL) {
        return;
    }
    int64_t date = khc_get_resp_date(&kii->_khc);
    if (date >= 0) {
        kii_clock_set_time(kii->_clock, date);
    }
}
