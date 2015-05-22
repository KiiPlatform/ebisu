#ifndef KII_UTILS_H
#define KII_UTILS_H

#include "kii.h"

typedef enum {
    T_NUMBER,
    T_BOOLEAN,
    T_STRING
} KII_JSON_ATTR_TYPE;

/** json parse attribute data */
typedef struct kii_json_attr {
    /** parse target key name */
    char* name;
    /** parse target value type */
    KII_JSON_ATTR_TYPE type;
    /** output parse value */
    void* out_value;
    /** output parse size(for only T_STRING) */
    size_t out_size;
} kii_json_attr_t;

/** parse json string by attribute.
 *  \param [in] kii sdk instance.
 *  \param [in] pointer of json string.
 *  \param [in] length of json string.
 *  \param [inout] attribute of kii json parser.
 *  \return 0:success -1:failure
 */
int prv_kii_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t string_len,
        kii_json_attr_t* attr);

#endif
