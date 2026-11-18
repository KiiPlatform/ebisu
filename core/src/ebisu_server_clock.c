#include "ebisu_server_clock.h"
#include "ebisu_clock_impl.h"

#include <string.h>

static ebisu_server_clock_t* _server_clock_of(ebisu_clock_t* self)
{
    return EBISU_CONTAINER_OF(self, ebisu_server_clock_t, base);
}

static bool _has_monotonic(const ebisu_server_clock_t* clock)
{
    return clock->_system != NULL && clock->_system->ops->monotonic_ms != NULL;
}

static uint64_t _monotonic_ms(ebisu_server_clock_t* clock)
{
    return ebisu_system_monotonic_ms(clock->_system);
}

/* With the lock held: the time counted on from the one given, and in
   out_exact whether it is the time or a lower bound. A monotonic clock that went
   back, a 32-bit tick counter wrapping for one, can no longer count on from the
   time given, which the time is at least, though: it becomes the floor, and the
   time is forgotten, until the next. false when there is no time. */
static bool _counted(ebisu_server_clock_t* clock, int64_t* out_counted, bool* out_exact)
{
    if (!clock->_time_known || !_has_monotonic(clock)) {
        return false;
    }
    uint64_t now_ms = _monotonic_ms(clock);
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

/* With the lock held: the time is now, from this monotonic time on. */
static void _give(ebisu_server_clock_t* clock, int64_t now)
{
    clock->_time = now;
    clock->_time_at_ms = _monotonic_ms(clock);
    clock->_time_known = true;
}

static bool _now(ebisu_clock_t* self, int64_t* out_now)
{
    ebisu_server_clock_t* clock = _server_clock_of(self);
    bool exact = false;
    ebisu_lock_acquire(clock->_lock);
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
    ebisu_lock_release(clock->_lock);
    *out_now = now;
    return exact;
}

static void _stated(ebisu_clock_t* self, int64_t seconds)
{
    ebisu_server_clock_set_time(_server_clock_of(self), seconds);
}

/* The time forgotten, what was counted kept as the floor. */
static void _doubt(ebisu_clock_t* self)
{
    ebisu_server_clock_t* clock = _server_clock_of(self);
    ebisu_lock_acquire(clock->_lock);
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
    ebisu_lock_release(clock->_lock);
}

static const ebisu_clock_ops_t _server_clock_ops = {
    _now,
    _stated,
    _doubt
};

bool ebisu_server_clock_init(ebisu_server_clock_t* clock, ebisu_system_t* system)
{
    memset(clock, 0, sizeof(*clock));
    clock->base.ops = &_server_clock_ops;
    clock->_floor = _EBISU_CLOCK_FLOOR;
    clock->_time_known = false;
    clock->_exact_for_ms = 24ULL * 3600 * 1000;
    clock->_system = system;
    return _has_monotonic(clock);
}

void ebisu_server_clock_set_lock(ebisu_server_clock_t* clock, ebisu_lock_t* lock)
{
    clock->_lock = lock;
}

void ebisu_server_clock_set_exact_for(ebisu_server_clock_t* clock, uint32_t seconds)
{
    ebisu_lock_acquire(clock->_lock);
    clock->_exact_for_ms = (uint64_t)seconds * 1000;
    ebisu_lock_release(clock->_lock);
}

void ebisu_server_clock_set_floor(ebisu_server_clock_t* clock, int64_t floor)
{
    ebisu_lock_acquire(clock->_lock);
    if (floor > clock->_floor) {
        clock->_floor = floor;
    }
    ebisu_lock_release(clock->_lock);
}

bool ebisu_server_clock_set_time(ebisu_server_clock_t* clock, int64_t now)
{
    bool taken = false;
    ebisu_lock_acquire(clock->_lock);
    if (now >= clock->_floor && _has_monotonic(clock)) {
        _give(clock, now);
        taken = true;
    }
    ebisu_lock_release(clock->_lock);
    return taken;
}

bool ebisu_server_clock_override_time(ebisu_server_clock_t* clock, int64_t now)
{
    bool taken = false;
    ebisu_lock_acquire(clock->_lock);
    if (_has_monotonic(clock)) {
        if (now < clock->_floor) {
            clock->_floor = now;
        }
        _give(clock, now);
        taken = true;
    }
    ebisu_lock_release(clock->_lock);
    return taken;
}
