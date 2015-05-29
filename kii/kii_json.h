#ifndef KII_JSON_H
#define KII_JSON_H

#include "kii.h"

/** Return value of kii_json_read_object(kii_t*, const char*, size_t,
 * kii_json_field_t*) */
typedef enum kii_json_parse_result {

    /** JSON string is successfully parsed and all kii_json_field_t
     * variables are successfully set. i.e., all kii_json_field_t type
     * fields are KII_JSON_FIELD_PARSE_SUCCESS.
     */
    KII_JSON_PARSE_SUCCESS,

    /** JSON string is successfully parsed but some kii_json_field_t
     * variables are failed. i.e., some kii_json_field_t type fields
     * are not KII_JSON_FIELD_PARSE_SUCCESS.
     */
    KII_JSON_PARSE_PARTIAL_SUCCESS,

    /** JSON string is successfully parsed but type of root object
     * type is unmatched to using function.
     *
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*) requires JSON object if JSON array is
     * passed, then this error is returned.
     */
    KII_JSON_PARSE_ROOT_TYPE_ERROR,

    /** JSON string is failed to parse. Passed string is not JSON string. */
    KII_JSON_PARSE_INVALID_INPUT
} kii_json_parse_result_t;

/** Field parsing result. Assigned to kii_json_field_t#result. */
typedef enum kii_json_field_parse_result {
    /** Field parsing is success. */
    KII_JSON_FIELD_PARSE_SUCCESS,

    /** Type of field specified kii_json_field_t#type is unmathced.*/
    KII_JSON_FIELD_PARSE_TYPE_UNMATCHED,

    /** Field specified by kii_json_field_t#name is not found. */
    KII_JSON_FIELD_PARSE_NOT_FOUND,

    /** Coping string to kii_json_field_t#field_copy_buff is failed.
     * kii_json_field_t#field_copy_buff_size may shorter than actual
     * length.
     */
    KII_JSON_FIELD_PARSE_COPY_FAILED
} kii_json_field_parse_result_t;

/** Type of parsed JSON field. This value is assigned to
 * kii_json_field_t#type. */
typedef enum kii_json_field_type {

    /** This value denotes any JSON types. If this value is set to
     * kii_json_field_t#type, then kii_json_read_object(kii_t*, const
     * char*, size_t, kii_json_field_t*) ignore type checking.
     */
    KII_JSON_FIELD_TYPE_ANY,

    /** This value denotes JSON primitive values such as number and null. */
    KII_JSON_FIELD_TYPE_PRIMITIVE,

    /** This value denotes JSON string. */
    KII_JSON_FIELD_TYPE_STRING,

    /** This value denotes JSON object. */
    KII_JSON_FIELD_TYPE_OBJECT,

    /** This value denotes JSON array. */
    KII_JSON_FIELD_TYPE_ARRAY
} kii_json_field_type_t;

/** JSON parsed field data */
typedef struct kii_json_field {

    /** parsing target key name. Input of
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*).
     */
    const char* name;

    /** field parse result. Output of kii_json_read_object(kii_t*,
     * const char*, size_t, kii_json_field_t*).
     */
    kii_json_field_parse_result_t result;

    /** parsed target value type. Input and Output of
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*).
     *
     * If type is set except for
     * kii_json_field_type_t#KII_JSON_FIELD_TYPE_ANY, then
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*) ignore type checking.
     *
     * If actual type is not matched expected type:
     *   - kii_json_read_object(kii_t*, const char*, size_t,
     *     kii_json_field_t*) set actual type.
     *   - if expected type is not
     *     kii_json_field_type_t#KII_JSON_FIELD_TYPE_ANY, then
     *     kii_json_field_t#result become
     *     kii_json_parse_result_t#KII_JSON_FIELD_PARSE_TYPE_UNMATCHED.
     *   - if expected type is
     *     kii_json_field_type_t#KII_JSON_FIELD_TYPE_ANY, then
     *     kii_json_field_t#result become
     *     kii_json_parse_result_t#KII_JSON_FIELD_PARSE_SUCCESS.
     */
    kii_json_field_type_t type;

    /** start point of this field in given buffer. Output of
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*).
     */
    size_t start;

    /** end point of this field in given buffer. Output of
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*).
     */
    size_t end;

    /** buffer to copy field value. if null no copy is generated. */
    char* field_copy_buff;

    /** length of field_copy_buff. ignored if field_copy_buff is null. */
    size_t field_copy_buff_size;

} kii_json_field_t;

/** parse JSON string as JSON object.
 *  \param [in] kii sdk instance.
 *  \param [in] pointer of JSON string.
 *  \param [in] length of JSON string.
 *  \param [inout] field of kii JSON parser.
 *  \return parse JSON result.
 */
kii_json_parse_result_t kii_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_len,
        kii_json_field_t* fields);

#endif
