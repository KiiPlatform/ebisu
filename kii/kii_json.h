#ifndef KII_JSON_H
#define KII_JSON_H

/** object manages context of kii json apis. */
typedef struct kii_json_t {

    /** Number of json tokens. */
    int json_token_num;

    /** Error string. If error occurs in kii_json library, then error
     * message is set to this fields. If NULL, no error message is
     * set.
     */
    char* error_string_buff;

    /** Size of error_string_buff. */
    size_t error_string_length;
} kii_json_t;

/** Boolean type */
typedef enum kii_json_boolean {
    KII_JSON_FALSE,
    KII_JSON_TRUE
} kii_json_boolean_t;

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

    /** Coping string to kii_json_field_t#field_copy#string is failed.
     * kii_json_field_t#field_copy_buff_size may shorter than actual
     * length.
     */
    KII_JSON_FIELD_PARSE_COPY_FAILED,

    /** Coping int or long value to
     * kii_json_field_t#field_copy#int_value or
     * kii_json_field_t#field_copy#long_value is failed. value is
     * overflowed.
     */
    KII_JSON_FIELD_PARSE_COPY_OVERFLOW,

    /** Coping int or long value to
     * kii_json_field_t#field_copy#int_value or
     * kii_json_field_t#field_copy#long_value is failed. value is
     * underflowed.
     */
    KII_JSON_FIELD_PARSE_COPY_UNDERFLOW
} kii_json_field_parse_result_t;

/** Type of parsed JSON field. This value is assigned to
 * kii_json_field_t#type. */
typedef enum kii_json_field_type {

    /** This value denotes any JSON types. If this value is set to
     * kii_json_field_t#type, then kii_json_read_object(kii_t*, const
     * char*, size_t, kii_json_field_t*) ignore type checking.
     */
    KII_JSON_FIELD_TYPE_ANY,

    /** This value denotes JSON primitive values such as number,
     * boolean and null. */
    KII_JSON_FIELD_TYPE_PRIMITIVE,

    /** This values denotes an signed interger value. Maximum is
     * INT_MAX and Minimum is INT_MIN. */
    KII_JSON_FIELD_TYPE_INTEGER,

    /** This values denotes an signed interger value. Maximum is
     * LONG_MAX and Minimum is LONG_MIN. */
    KII_JSON_FIELD_TYPE_LONG,

    /** This value denotes an double value. */
    KII_JSON_FIELD_TYPE_DOUBLE,

    /** This value denotes kii_json_boolean_t value. */
    KII_JSON_FIELD_TYPE_BOOLEAN,

    /** This value denotes denotes NULL value. */
    KII_JSON_FIELD_TYPE_NULL,

    /** This value denotes JSON string. */
    KII_JSON_FIELD_TYPE_STRING,

    /** This value denotes JSON object. */
    KII_JSON_FIELD_TYPE_OBJECT,

    /** This value denotes JSON array. */
    KII_JSON_FIELD_TYPE_ARRAY
} kii_json_field_type_t;

/** JSON parsed field data */
typedef struct kii_json_field {

    /** Parsing target key name. Input of
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*).
     */
    const char* name;

    /** Field parse result. Output of kii_json_read_object(kii_t*,
     * const char*, size_t, kii_json_field_t*).
     */
    kii_json_field_parse_result_t result;

    /** Parsed target value type. Input and Output of
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*). Inputted value is expected value type and
     * outputted value is actual value type.
     *
     * If type is set as
     * kii_json_field_type_t#KII_JSON_FIELD_TYPE_ANY, then
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*) ignore type checking.
     *
     * If actual type is not matched expected type:
     *   - kii_json_read_object(kii_t*, const char*, size_t,
     *     kii_json_field_t*) set actual type.
     *   - if expected type is not
     *     kii_json_field_type_t#KII_JSON_FIELD_TYPE_ANY, then
     *     kii_json_field_t#result becomes
     *     kii_json_parse_result_t#KII_JSON_FIELD_PARSE_TYPE_UNMATCHED.
     *   - if expected type is
     *     kii_json_field_type_t#KII_JSON_FIELD_TYPE_ANY, then
     *     kii_json_field_t#result become
     *     kii_json_parse_result_t#KII_JSON_FIELD_PARSE_SUCCESS.
     *
     * If expected type is
     * kii_json_field_type_t#KII_JSON_FIELD_TYPE_PRIMITIVE and Actual
     * type is one of
     * kii_json_field_type_t#KII_JSON_FIELD_TYPE_INTEGER,
     * kii_json_field_type_t#KII_JSON_FIELD_TYPE_LONG,
     * kii_json_field_type_t#KII_JSON_FIELD_TYPE_BOOLEAN or
     * kii_json_field_type_t#KII_JSON_FIELD_TYPE_NULL:
     *   - kii_json_field_t#result becomes
     *     kii_json_parse_result_t#KII_JSON_FIELD_PARSE_SUCCESS,
     *     otherwise,
     *     kii_json_parse_result_t#KII_JSON_FIELD_PARSE_TYPE_UNMATCHED.
     *   - Outputted actual type is
     *     kii_json_field_type_t#KII_JSON_FIELD_TYPE_PRIMITIVE.
     */
    kii_json_field_type_t type;

    /** Start point of this field in given buffer. Output of
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*).
     */
    size_t start;

    /** End point of this field in given buffer. Output of
     * kii_json_read_object(kii_t*, const char*, size_t,
     * kii_json_field_t*).
     */
    size_t end;

    /** Buffer to copy field value. if NULL, no copy is
     * generated. Using value is determined by value of
     * kii_json_field_t#type. If kii_json_field_t#type is
     * kii_json_field_type_t#KII_JSON_FIELD_TYPE_NULL, no copy is
     * generated.
     */
    union {

        /** This value is used if kii_json_field_t#type is
         * kii_json_field_type_t#KII_JSON_FIELD_TYPE_STRING,
         * kii_json_field_type_t#KII_JSON_FIELD_TYPE_PRIMITIVE,
         * KII_JSON_FIELD_TYPE_OBJECT or KII_JSON_FIELD_TYPE_ARRAY.
         */
        char* string;

        /** This value is used if kii_json_field_t#type is
         * kii_json_field_type_t#KII_JSON_FIELD_TYPE_INTEGER.
         */
        int* int_value;

        /** This value is used if kii_json_field_t#type is
         * kii_json_field_type_t#KII_JSON_FIELD_TYPE_LONG.
         */
        long* long_value;

        /** This value is used if kii_json_field_t#type is
         * kii_json_field_type_t#KII_JSON_FIELD_TYPE_DOUBLE.
         */
        double* double_value;

        /** This value is used if kii_json_field_t#type is
         * kii_json_field_type_t#KII_JSON_FIELD_TYPE_BOOLEAN.
         */
        kii_json_boolean_t* boolean_value;
    } field_copy;

    /** Length of field_copy#string. ignored if field_copy#string is
     * null or kii_json_field_t#type is not
     * kii_json_field_type_t#KII_JSON_FIELD_TYPE_STRING and
     * kii_json_field_type_t#KII_JSON_FIELD_TYPE_PRIMITIVE. */
    size_t field_copy_buff_size;

} kii_json_field_t;

/** parse JSON string as JSON object.
 *  \param [in] kii json instance.
 *  \param [in] pointer of JSON string.
 *  \param [in] length of JSON string.
 *  \param [inout] field of kii JSON parser.
 *  \return parse JSON result.
 */
kii_json_parse_result_t kii_json_read_object(
        kii_json_t* kii_json,
        const char* json_string,
        size_t json_string_len,
        kii_json_field_t* fields);

#endif
