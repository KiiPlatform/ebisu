#ifndef __kii_impl__
#define __kii_impl__

#include "kii.h"
#include "khc.h"

int _kii_set_content_length(kii_t* kii, size_t content_length);

kii_code_t _convert_code(khc_code khc_c);

const char _APP_KEY_HEADER[] = "X-Kii-Appkey: k";

#endif