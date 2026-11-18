#include "kii.h"
#include "kii_impl.h"

kii_code_t kii_set_clock(kii_t* kii, ebisu_clock_t* clock)
{
    if (clock != NULL && (clock->ops == NULL || clock->ops->now == NULL)) {
        return KII_ERR_FAIL;
    }
    kii->_clock = clock;
    return KII_ERR_OK;
}

void _kii_clock_record(kii_t* kii)
{
    /* A time is only as good as the connection it came through: none from
     * plain HTTP, where anyone on the way can write it. */
    if (kii->_clock == NULL || kii->_clock->ops->stated == NULL ||
            kii->_khc._enable_insecure) {
        return;
    }
    int64_t date = khc_get_resp_date(&kii->_khc);
    if (date >= 0) {
        kii->_clock->ops->stated(kii->_clock, date);
    }
}
