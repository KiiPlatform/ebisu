#include "ebisu_clock.h"
#include "ebisu_clock_impl.h"

#include <stddef.h>

bool ebisu_clock_get_time(ebisu_clock_t* clock, int64_t* out_now)
{
    return clock->ops->now(clock, out_now);
}

void ebisu_clock_forget_time(ebisu_clock_t* clock)
{
    if (clock->ops->doubt != NULL) {
        clock->ops->doubt(clock);
    }
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

bool ebisu_clock_from_system_time(int64_t seconds, int64_t* out_now)
{
    if (seconds < _EBISU_CLOCK_FLOOR) {
        *out_now = _EBISU_CLOCK_FLOOR;
        return false;
    }
    *out_now = seconds;
    return true;
}
