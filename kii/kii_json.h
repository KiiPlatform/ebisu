#ifndef KII_JSON_H
#define KII_JSON_H

#include "kii.h"

typedef enum kii_json_result {
    KII_JSON_SUCCESS,
    KII_JSON_UNEXPECTED_ARRAY,
    KII_JSON_UNEXPECTED_OBJECT,
    KII_JSON_INVALID
} kii_json_result_t;

typedef enum kii_json_attr_result {
    KII_JSON_ATTR_SUCCESS,
    KII_JSON_ATTR_UNMATCH_TYPE,
    KII_JSON_ATTR_IS_NULL,
    KII_JSON_ATTR_NOT_FOUND
} kii_json_attr_result_t;

typedef enum kii_json_boolean {
    KII_JSON_FALSE,
    KII_JSON_TRUE
} kii_json_boolean_t;

typedef enum kii_json_attr_type {
    KII_JSON_ATTR_TYPE_PRIMITIVE,
    KII_JSON_ATTR_TYPE_STRING
} kii_json_attr_type_t;

/** json parsed attribute data */
typedef struct kii_json_attr {
    /** parsing target key name */
    char* name;
    /** attribute parse result */
    kii_json_attr_result_t result;
    /** parsed target value type */
    kii_json_attr_type_t type;
    /** start point of this attribute in given buffer */
    int start;
    /** end point of this attribute in given buffer */
    int end;
} kii_json_attr_t;

/** parse json string by attribute.
 *  \param [in] kii sdk instance.
 *  \param [in] pointer of json string.
 *  \param [in] length of json string.
 *  \param [inout] attribute of kii json parser.
 *  \return parse json result.
 */
kii_json_result_t prv_kii_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t string_len,
        kii_json_attr_t* attr);

// TODO: remove this after new api is implemented..
int prv_kii_jsmn_get_tokens(
        kii_t* kii,
        const char* json_string,
        size_t json_string_len,
        jsmntok_t* tokens,
        size_t token_num);

// TODO: remove this after new api is implemented.
int prv_kii_jsmn_get_value(
        const char* json_string,
        size_t json_string_len,
        const jsmntok_t* tokens,
        const char* name,
        jsmntok_t** out_token);

#endif
