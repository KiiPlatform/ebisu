#ifndef KII_JSON_H
#define KII_JSON_H

#include "kii.h"

typedef enum kii_json_parse_result {
    KII_JSON_PARSE_SUCCESS,
    KII_JSON_PARSE_PARTIAL_SUCCESS,
    KII_JSON_PARSE_ROOT_TYPE_ERROR,
    KII_JSON_PARSE_INVALID_INPUT
} kii_json_parse_result_t;

typedef enum kii_json_field_parse_result {
    KII_JSON_FIELD_PARSE_SUCCESS,
    KII_JSON_FIELD_PARSE_TYPE_UNMATCHED,
    KII_JSON_FIELD_PARSE_NOT_FOUND,
    KII_JSON_FIELD_PARSE_COPY_FAILED
} kii_json_field_parse_result_t;

typedef enum kii_json_field_type {
    KII_JSON_FIELD_TYPE_PRIMITIVE,
    KII_JSON_FIELD_TYPE_STRING,
    KII_JSON_FIELD_TYPE_OBJECT,
    KII_JSON_FIELD_TYPE_ARRAY
} kii_json_field_type_t;

/** json parsed field data */
typedef struct kii_json_field {

    /** parsing target key name. Input of kii_json_read_object. */
    const char* name;

    /** field parse result. Output of kii_json_read_object. */
    kii_json_field_parse_result_t result;

    /** parsed target value type. Input and Output of kii_json_read_object. */
    kii_json_field_type_t type;

    /** start point of this field in given buffer.
     * Output of kii_json_read_object
     */
    size_t start;

    /** end point of this field in given buffer.
     * Output of kii_json_read_object
     */
    size_t end;

    /** buffer to copy field value. if null no copy is generated. */
    char* field_copy_buff;

    /** length of field_copy_buff. ignored if field_copy_buff is null. */
    size_t field_copy_buff_size;

} kii_json_field_t;

/** parse json string as json object.
 *  \param [in] kii sdk instance.
 *  \param [in] pointer of json string.
 *  \param [in] length of json string.
 *  \param [inout] field of kii json parser.
 *  \return parse json result.
 */
kii_json_parse_result_t kii_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_len,
        kii_json_field_t* fields);

#endif
