#include "kii.h"
#include "kii_impl.h"

kii_code_t kii_set_clock(kii_t* kii, ebisu_clock_t* clock)
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
        ebisu_clock_set_time(kii->_clock, date);
    }
}
