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

#ifdef EBISU_NO_LOG
/* Built without logging, nothing is logged, and the arguments aren't
   evaluated. */
#define _tio_log(kii, level, ...) ((void)0)
#else
/* Logs a message with the logger set on kii, if any. */
void _tio_log(kii_t* kii, ebisu_log_level_t level, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;
#endif

#ifdef __cplusplus
}
#endif

#endif
