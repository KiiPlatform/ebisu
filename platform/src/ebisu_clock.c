#include "ebisu_clock.h"
#include "ebisu_version.h"

#include <string.h>

static void _lock_clock(ebisu_clock_t* clock)
{
    if (clock->_cb_lock != NULL) {
        clock->_cb_lock(clock->_lock_data);
    }
}

static void _unlock_clock(ebisu_clock_t* clock)
{
    if (clock->_cb_unlock != NULL) {
        clock->_cb_unlock(clock->_lock_data);
    }
}

void ebisu_clock_init(
        ebisu_clock_t* clock,
        EBISU_CB_MONOTONIC_MS cb_monotonic_ms,
        void* userdata)
{
    memset(clock, 0, sizeof(*clock));
    /* The floor khc_init() sets khc: a week's margin, for a committer whose
     * clock was a little ahead. */
    clock->_floor = EBISU_SOURCE_TIME - 7 * 86400;
    clock->_time_known = false;
    clock->_exact_for_ms = 24ULL * 3600 * 1000;
    clock->_cb_monotonic_ms = cb_monotonic_ms;
    clock->_monotonic_ms_data = userdata;
}

void ebisu_clock_set_cb_lock(
        ebisu_clock_t* clock,
        EBISU_CB_LOCK cb_lock,
        EBISU_CB_LOCK cb_unlock,
        void* userdata)
{
    clock->_cb_lock = cb_lock;
    clock->_cb_unlock = cb_unlock;
    clock->_lock_data = userdata;
}

void ebisu_clock_set_exact_for(ebisu_clock_t* clock, uint32_t seconds)
{
    _lock_clock(clock);
    clock->_exact_for_ms = (uint64_t)seconds * 1000;
    _unlock_clock(clock);
}

void ebisu_clock_set_floor(ebisu_clock_t* clock, int64_t floor)
{
    _lock_clock(clock);
    if (floor > clock->_floor) {
        clock->_floor = floor;
    }
    _unlock_clock(clock);
}

bool ebisu_clock_set_time(ebisu_clock_t* clock, int64_t now)
{
    bool taken = false;
    _lock_clock(clock);
    if (now >= clock->_floor && clock->_cb_monotonic_ms != NULL) {
        clock->_time = now;
        clock->_time_at_ms = clock->_cb_monotonic_ms(clock->_monotonic_ms_data);
        clock->_time_known = true;
        taken = true;
    }
    _unlock_clock(clock);
    return taken;
}

/* With the lock held: the time counted on from the one given, and in
 * out_exact whether it is the time or a lower bound. A monotonic clock that went
 * back, a 32-bit tick counter wrapping for one, can no longer count on from the
 * time given, which the time is at least, though: it becomes the floor, and the
 * time is forgotten, until the next. false when there is no time. */
static bool _counted(ebisu_clock_t* clock, int64_t* out_counted, bool* out_exact)
{
    if (!clock->_time_known || clock->_cb_monotonic_ms == NULL) {
        return false;
    }
    uint64_t now_ms = clock->_cb_monotonic_ms(clock->_monotonic_ms_data);
    if (now_ms < clock->_time_at_ms) {
        if (clock->_time > clock->_floor) {
            clock->_floor = clock->_time;
        }
        clock->_time_known = false;
        return false;
    }
    uint64_t elapsed_ms = now_ms - clock->_time_at_ms;
    int64_t elapsed = (int64_t)(elapsed_ms / 1000);
    *out_exact = elapsed_ms < clock->_exact_for_ms;
    /* Long after the time was given, what a monotonic clock counts is a lower
     * bound at best: one stopped while the device slept counts less than has
     * passed, and one running fast, by up to 1% for an RC oscillator, more. */
    *out_counted = clock->_time + elapsed - (*out_exact ? 0 : elapsed / 100);
    return true;
}

bool ebisu_clock_get_time(ebisu_clock_t* clock, int64_t* out_now)
{
    bool exact = false;
    _lock_clock(clock);
    int64_t counted = 0;
    bool fresh = false;
    bool has_time = _counted(clock, &counted, &fresh);
    /* After _counted(), which can raise the floor. A floor later than the time
     * counted on is the better bound. */
    int64_t now = clock->_floor;
    if (has_time && counted >= now) {
        now = counted;
        exact = fresh;
    }
    _unlock_clock(clock);
    *out_now = now;
    return exact;
}

void ebisu_clock_forget_time(ebisu_clock_t* clock)
{
    _lock_clock(clock);
    int64_t counted = 0;
    bool fresh = false;
    if (_counted(clock, &counted, &fresh)) {
        /* What was counted, less 1% however fresh: a lower bound of the time
         * however the monotonic clock ran, unless it stopped, which counts
         * less still. */
        if (fresh) {
            int64_t elapsed = counted - clock->_time;
            counted -= elapsed / 100;
        }
        if (counted > clock->_floor) {
            clock->_floor = counted;
        }
        clock->_time_known = false;
    }
    _unlock_clock(clock);
}

ebisu_validity_t ebisu_clock_check_validity(
        ebisu_clock_t* clock,
        int64_t not_before,
        int64_t not_after)
{
    int64_t now = 0;
    bool exact = ebisu_clock_get_time(clock, &now);
    if (now > not_after) {
        return EBISU_VALIDITY_EXPIRED;
    }
    if (exact && now < not_before) {
        return EBISU_VALIDITY_NOT_YET_VALID;
    }
    return EBISU_VALIDITY_OK;
}
