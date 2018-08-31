#include "kii.h"

int kii_api_call(
    kii_t *kii,
    const char *http_method,
    const char *resource_path,
    const void *http_body,
    size_t body_size,
    const char *content_type,
    char *header,
    ...)
{
    // TODO: reimplement it. Equivalent to kii_core_api_call().
    return -1;
}
