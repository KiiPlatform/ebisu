#ifndef _jkii_utils_
#define _jkii_utils_

#ifdef __cplusplus
extern "C" {
#endif

#include "jkii.h"

#ifdef ERRNO_HACK_NEEDED
int errno;
#define EINVAL 22
#define ERANGE 34
#endif

typedef enum jkii_primitive_err_t
{
    JKII_PRIMITIVE_ERR_OK,
    JKII_PRIMITIVE_ERR_OVERFLOW,
    JKII_PRIMITIVE_ERR_UNDERFLOW,
    JKII_PRIMITIVE_ERR_INVALID
} jkii_primitive_err_t;

/** \brief A parsed JSON primitive: its value together with the type read.
 *
 * Filled in by jkii_parse_primitive(). Read the member of #value that #type
 * selects; the others are not meaningful.
 */
typedef struct jkii_primitive_t {
    /** \brief The value. Which member is valid is given by #type. */
    union {
        int int_value; /**< \brief Valid when #type is JKII_FIELD_TYPE_INTEGER. */
        long long_value; /**< \brief Valid when #type is JKII_FIELD_TYPE_LONG. */
        double double_value; /**< \brief Valid when #type is JKII_FIELD_TYPE_DOUBLE. */
        jkii_boolean_t boolean_value; /**< \brief Valid when #type is JKII_FIELD_TYPE_BOOLEAN. */
    } value;
    /** \brief Type of the primitive that was parsed, selecting a member of #value.
     *
     * JKII_FIELD_TYPE_NULL indicates the JSON literal null, in which case no
     * member of #value is set.
     */
    jkii_field_type_t type;
} jkii_primitive_t;

jkii_primitive_err_t jkii_parse_primitive(
    const char* primitive,
    size_t primitive_length,
    jkii_primitive_t* result);

int jkii_escape_str(const char* str, char* buff, size_t buff_size);

jkii_parse_err_t jkii_validate_root_object(
        const char* json_string,
        size_t json_string_len,
        jkii_resource_t* resource);

jkii_parse_err_t jkii_validate_root_object_with_allocator(
        const char* json_string,
        size_t json_string_len,
        JKII_CB_RESOURCE_ALLOC cb_alloc,
        JKII_CB_RESOURCE_FREE cb_free);

#ifdef __cplusplus
}
#endif
#endif
