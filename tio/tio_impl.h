#ifndef _tio_impl_
#define _tio_impl_

#ifdef __cplusplus
extern "C" {
#endif

#include "tio.h"
#include "kii.h"

tio_code_t _tio_convert_code(kii_code_t code);

/* Uploads the state once, if TIO_CB_SIZE reports any. */
kii_code_t _tio_updater_upload(tio_updater_t* updater);

#ifdef __cplusplus
}
#endif

#endif
