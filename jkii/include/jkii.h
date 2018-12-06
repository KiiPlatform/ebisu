/**
 * @file jkii.h
 * @brief This is a file defining Kii JSON APIs.
 */
#ifndef JKII_H
#define JKII_H

#include <jsmn.h>

#ifdef __cplusplus
extern "C" {
#endif

/** JSON token data to parse JSON string. */
typedef jsmntok_t jkii_token_t;

/** Resource used by KII JSON library. Fields of this struct
 * determines max number of tokens inside json can be parsed.
 */
typedef struct jkii_resource_t {

    /** Array to set jkii_t#tokens. */
    jkii_token_t *tokens;

    /** Size of jkii_resource_t#tokens */
    size_t tokens_num;

} jkii_resource_t;

/** Resource allocator for jkii_resource_t.
 * \param[in] required_size required token size.
 * \return jkii_resource_t instance of NULL if failed to allocate resource.
 */
typedef jkii_resource_t*
    (*JKII_CB_RESOURCE_ALLOC)(
        size_t required_size);

typedef void (*JKII_CB_RESOURCE_FREE)(
    jkii_resource_t* resource);

/** Boolean type */
typedef enum jkii_boolean_t {
    JKII_FALSE,
    JKII_TRUE
} jkii_boolean_t;

/** Return value of jkii_parse(jkii_t*, const char*,
 * size_t, jkii_field_t*) */
typedef enum jkii_parse_err_t {

    /** JSON string is successfully parsed and all jkii_field_t
     * variables are successfully set. i.e., all jkii_field_t type
     * fields are JKII_FIELD_ERR_OK.
     */
    JKII_ERR_OK,

    /** JSON string is successfully parsed but some jkii_field_t
     * variables are failed. i.e., some jkii_field_t type fields
     * are not JKII_FIELD_ERR_OK.
     */
    JKII_ERR_PARTIAL,

    /** JSON string is successfully parsed but type of root object
     * type is unmatched to using function.
     *
     * jkii_parse(jkii_t*, const char*, size_t,
     * jkii_field_t*) requires JSON object if JSON array is
     * passed, then this error is returned.
     */
    JKII_ERR_ROOT_TYPE,

    /** jkii_t#tokens is not enough to parse JSON string.*/
    JKII_ERR_TOKENS_SHORTAGE,

    /** JSON string is failed to parse. Passed string is not JSON string. */
    JKII_ERR_INVALID_INPUT,

    /** Allocation failed. */
    JKII_ERR_ALLOCATION,
} jkii_parse_err_t;

/** Field parsing result. Assigned to jkii_field_t#result. */
typedef enum jkii_field_err_t {
    /** Field parsing is success. */
    JKII_FIELD_ERR_OK,

    /** Type of field specified jkii_field_t#type is mismatch.*/
    JKII_FIELD_ERR_TYPE_MISMATCH,

    /** Field specified by jkii_field_t#name is not found. */
    JKII_FIELD_ERR_NOT_FOUND,

    /** Coping string to jkii_field_t#field_copy#string is failed.
     * jkii_field_t#field_copy_buff_size may shorter than actual
     * length.
     */
    JKII_FIELD_ERR_COPY,

    /** Coping int, long or double value to
     * jkii_field_t#field_copy#int_value,
     * jkii_field_t#field_copy#long_value or
     * jkii_field_t#field_copy#double_value is failed. value is
     * overflowed.
     */
    JKII_FIELD_ERR_NUM_OVERFLOW,

    /** Coping int, long or double value to
     * jkii_field_t#field_copy#int_value,
     * jkii_field_t#field_copy#long_value or
     * jkii_field_t#field_copy#double_value is failed. value is
     * underflowed.
     */
    JKII_FIELD_ERR_NUM_UNDERFLOW
} jkii_field_err_t;

/** Type of parsed JSON field. This value is assigned to
 * jkii_field_t#type. */
typedef enum jkii_field_type_t {

    /** This value denotes any JSON types. If this value is set to
     * jkii_field_t#type, then jkii_parse(jkii_t*,
     * const char*, size_t, jkii_field_t*) ignore type checking.
     */
    JKII_FIELD_TYPE_ANY,

    /** This values denotes an signed interger value. Maximum is
     * INT_MAX and Minimum is INT_MIN. */
    JKII_FIELD_TYPE_INTEGER,

    /** This values denotes an signed interger value. Maximum is
     * LONG_MAX and Minimum is LONG_MIN. */
    JKII_FIELD_TYPE_LONG,

    /** This value denotes an double value. */
    JKII_FIELD_TYPE_DOUBLE,

    /** This value denotes jkii_boolean_t value. */
    JKII_FIELD_TYPE_BOOLEAN,

    /** This value denotes denotes NULL value. */
    JKII_FIELD_TYPE_NULL,

    /** This value denotes JSON string. */
    JKII_FIELD_TYPE_STRING,

    /** This value denotes JSON object. */
    JKII_FIELD_TYPE_OBJECT,

    /** This value denotes JSON array. */
    JKII_FIELD_TYPE_ARRAY
} jkii_field_type_t;

/** JSON parsed field data.
 *
 * Input of jkii_parse(jkii_t*, const char*, size_t,
 * jkii_field_t*).
 *
 * Array of jkii_field_t is passed to
 * jkii_parse(jkii_t*, const char*, size_t,
 * jkii_field_t*).
 *

 * End point of the array is specified by jkii_field_t#name and
 * jkii_field_t#path. If both of jkii_field_t#name and
 * jkii_field_t#path are NULL, jkii_parse(jkii_t*,
 * const char*, size_t, jkii_field_t*) consider that it is the end
 * point of the passed array.
 */
typedef struct jkii_field_t {

    /** Parsing target key name. Input of
     * jkii_parse(jkii_t*, const char*, size_t,
     * jkii_field_t*).
     *
     * Addressing a top-level field in a json object.
     */
    const char* name;

    /** Parsing target path. Input of
     * jkii_parse(jkii_t*, const char*, size_t,
     * jkii_field_t*).
     *
     * This can point any field or element of array. BNF like notation
     * of path is following:
     *
     * \code
     * path ::= '/' identifier subpath
     * subpath ::= '/' identifier subpath | ''
     * identifier ::= field | index
     * index ::= '[' 0 ']' | '[' [1-9][0-9]+ ']'
     * field ::= [char | escaped]+
     * char ::= any ascii characters expect '/', '[', ']' and '\'
     * escaped = "\\/" | "\\[" | "\\]" | "\\"
     * \endcode
     *
     * If you want to get first element of color array in following
     * json example:
     *
     * \code
     * {
     *     "light" : {
     *         "color" : [ 0, 128, 255]
     *      }
     * }
     * \endcode
     *
     * \code
     * path ="/light/color/[0]";
     * \endcode
     *
     * In rare cases, like following:
     * \code
     * {
     *     "[]/\\" : "rare"
     * }
     * \endcode
     *
     * You can specify "[]/\\" as following:
     *
     * \code
     * path ="/\\[\\]\\/\\";
     * \endcode
     */
    const char* path;

    /** Field parse result. Output of
     * jkii_parse(jkii_t*, const char*, size_t,
     * jkii_field_t*).
     */
    jkii_field_err_t result;

    /** Parsed target value type. Input and Output of
     * jkii_parse(jkii_t*, const char*, size_t,
     * jkii_field_t*). Inputted value is expected value type and
     * outputted value is actual value type.
     *
     * If type is set as
     * jkii_field_type_t#JKII_FIELD_TYPE_ANY, then
     * jkii_parse(jkii_t*, const char*, size_t,
     * jkii_field_t*) ignore type checking.
     *
     * If actual type is not matched expected type:
     *   - jkii_parse(jkii_t*, const char*, size_t,
     *     jkii_field_t*) set actual type.
     *   - if expected type is not
     *     jkii_field_type_t#JKII_FIELD_TYPE_ANY, then
     *     jkii_field_t#result becomes
     *     jkii_parse_err_t#JKII_FIELD_ERR_TYPE_MISMATCH.
     *   - if expected type is
     *     jkii_field_type_t#JKII_FIELD_TYPE_ANY, then
     *     jkii_field_t#result become
     *     jkii_parse_err_t#JKII_FIELD_ERR_OK.
     */
    jkii_field_type_t type;

    /** Start point of this field in given buffer. Output of
     * jkii_parse(jkii_t*, const char*, size_t,
     * jkii_field_t*).
     */
    size_t start;

    /** End point of this field in given buffer. Output of
     * jkii_parse(jkii_t*, const char*, size_t,
     * jkii_field_t*).
     */
    size_t end;

    /** Buffer to copy field value. if NULL, no copy is
     * generated. Using value is determined by value of
     * jkii_field_t#type. If jkii_field_t#type is
     * jkii_field_type_t#JKII_FIELD_TYPE_NULL, no copy is
     * generated.
     */
    union {

        /** This value is used if jkii_field_t#type is
         * jkii_field_type_t#JKII_FIELD_TYPE_STRING,
         * jkii_field_type_t#JKII_FIELD_TYPE_OBJECT or
         * jkii_field_type_t#JKII_FIELD_TYPE_ARRAY.
         */
        char* string;

        /** This value is used if jkii_field_t#type is
         * jkii_field_type_t#JKII_FIELD_TYPE_INTEGER. If
         * parsing target is overflowed, then this value is
         * INT_MAX. If parsing is underflowed, then this value is
         * INT_MIN.
         */
        int int_value;

        /** This value is used if jkii_field_t#type is
         * jkii_field_type_t#JKII_FIELD_TYPE_LONG. If parsing
         * target is overflowed, then this value is LONG_MAX. If parsing
         * is underflowed, then this value is LONG_MIN.
         */
        long long_value;

        /** This value is used if jkii_field_t#type is
         * jkii_field_type_t#JKII_FIELD_TYPE_DOUBLE. If
         * parsing target is overflowed, then this value is plus or
         * minus HUGE_VAL. If parsing is underflowed, then this value is
         * 0.
         */
        double double_value;

        /** This value is used if jkii_field_t#type is
         * jkii_field_type_t#JKII_FIELD_TYPE_BOOLEAN.
         */
        jkii_boolean_t boolean_value;
    } field_copy;

    /** Length of field_copy#string. ignored if field_copy#string is
     * null or jkii_field_t#type is not
     * jkii_field_type_t#JKII_FIELD_TYPE_STRING,
     * jkii_field_type_t#JKII_FIELD_TYPE_OBJECT and
     * jkii_field_type_t#JKII_FIELD_TYPE_ARRAY.
     */
    size_t field_copy_buff_size;

} jkii_field_t;

/** Parse JSON string.
 *  \param [in] json_string of JSON string.
 *  \param [in] json_string_len of JSON string.
 *  \param [in,out] fields of kii JSON parser.
 *  \param [in] resource of parser.
 *  \return parse result.
 */
jkii_parse_err_t jkii_parse(
        const char* json_string,
        size_t json_string_len,
        jkii_field_t* fields,
        jkii_resource_t* resource);

/** Parse JSON string with custom memory allocator.
 *  \param [in] json_string of JSON string.
 *  \param [in] json_string_len of JSON string.
 *  \param [in,out] fields of kii JSON parser.
 *  \param [in] cb_alloc allocate resource
 *  \param [in] cb_free free resource
 *  \return parse result.
 */
jkii_parse_err_t jkii_parse_with_allocator(
    const char* json_string,
    size_t json_string_len,
    jkii_field_t* fields,
    JKII_CB_RESOURCE_ALLOC cb_alloc,
    JKII_CB_RESOURCE_FREE cb_free);

#ifdef __cplusplus
}
#endif

#endif
