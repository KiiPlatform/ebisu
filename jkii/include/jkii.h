/**
 * @file jkii.h
 * @brief This is a file defining Kii JSON APIs.
 */
#ifndef JKII_H
#define JKII_H

#include <stddef.h>
#include <stdint.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what the shared object exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif


#ifdef __cplusplus
extern "C" {
#endif

/** Boolean type */
typedef enum jkii_boolean_t {
    JKII_FALSE,
    JKII_TRUE
} jkii_boolean_t;

/** Return value of jkii_parse() */
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
     * jkii_validate_root_object() requires a JSON object, and returns this
     * error for anything else, and jkii_next_member() an object or an array.
     */
    JKII_ERR_ROOT_TYPE,

    /** The string is not JSON, as ECMA-404 has it, or nests objects and
     * arrays deeper than 32 levels.
     */
    JKII_ERR_INVALID_INPUT,

    /** jkii_next_member() has given every member there is. */
    JKII_ERR_NOT_FOUND
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
     * jkii_field_t#type, then jkii_parse() ignore type checking.
     */
    JKII_FIELD_TYPE_ANY,

    /** This value denotes a signed integer held as an int32_t: from
     * INT32_MIN to INT32_MAX, whatever the width of int. */
    JKII_FIELD_TYPE_INTEGER,

    /** This value denotes a signed integer held as an int64_t: from
     * INT64_MIN to INT64_MAX, whatever the width of long. */
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
 * Input of jkii_parse().
 *
 * Array of jkii_field_t is passed to
 * jkii_parse().
 *

 * End point of the array is specified by jkii_field_t#name and
 * jkii_field_t#path. If both of jkii_field_t#name and
 * jkii_field_t#path are NULL, jkii_parse() consider that it is the end
 * point of the passed array.
 */
typedef struct jkii_field_t {

    /** Parsing target key name. Input of
     * jkii_parse().
     *
     * Addressing a top-level field in a json object.
     */
    const char* name;

    /** Parsing target path. Input of
     * jkii_parse().
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
     * You can specify that key as following:
     *
     * \code
     * path ="/\\[\\]\\/\\";
     * \endcode
     */
    const char* path;

    /** Field parse result. Output of
     * jkii_parse(), which sets it whatever it returns:
     * jkii_field_err_t#JKII_FIELD_ERR_NOT_FOUND for each field when it
     * returns jkii_parse_err_t#JKII_ERR_INVALID_INPUT.
     */
    jkii_field_err_t result;

    /** Parsed target value type. Input and Output of
     * jkii_parse(). Inputted value is expected value type and
     * outputted value is actual value type.
     *
     * If type is set as
     * jkii_field_type_t#JKII_FIELD_TYPE_ANY, then
     * jkii_parse() ignore type checking.
     *
     * If actual type is not matched expected type:
     *   - jkii_parse() set actual type.
     *   - if expected type is not
     *     jkii_field_type_t#JKII_FIELD_TYPE_ANY, then
     *     jkii_field_t#result becomes
     *     jkii_parse_err_t#JKII_FIELD_ERR_TYPE_MISMATCH.
     *   - if expected type is
     *     jkii_field_type_t#JKII_FIELD_TYPE_ANY, then
     *     jkii_field_t#result become
     *     jkii_parse_err_t#JKII_FIELD_ERR_OK.
     *
     * The actual type of an integer follows its value:
     * jkii_field_type_t#JKII_FIELD_TYPE_INTEGER if it fits in an int32_t,
     * jkii_field_type_t#JKII_FIELD_TYPE_LONG if it fits only in an int64_t.
     * One that fits in neither is
     * jkii_field_type_t#JKII_FIELD_TYPE_LONG with jkii_field_t#result
     * jkii_field_err_t#JKII_FIELD_ERR_NUM_OVERFLOW or
     * jkii_field_err_t#JKII_FIELD_ERR_NUM_UNDERFLOW. Expected as
     * jkii_field_type_t#JKII_FIELD_TYPE_INTEGER or
     * jkii_field_type_t#JKII_FIELD_TYPE_LONG, any integer matches, and one
     * past the range of the expected type is reported as overflow or
     * underflow.
     */
    jkii_field_type_t type;

    /** Start point of this field in given buffer. Output of
     * jkii_parse().
     */
    size_t start;

    /** End point of this field in given buffer. Output of
     * jkii_parse().
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
         * jkii_field_type_t#JKII_FIELD_TYPE_ARRAY. A string's characters
         * between its quotes are copied as written, escapes and all, which
         * jkii_unescape_str() undoes.
         */
        char* string;

        /** This value is used if jkii_field_t#type is
         * jkii_field_type_t#JKII_FIELD_TYPE_INTEGER. If
         * parsing target is overflowed, then this value is
         * INT32_MAX. If parsing is underflowed, then this value is
         * INT32_MIN.
         */
        int32_t int_value;

        /** This value is used if jkii_field_t#type is
         * jkii_field_type_t#JKII_FIELD_TYPE_LONG. If parsing
         * target is overflowed, then this value is INT64_MAX. If parsing
         * is underflowed, then this value is INT64_MIN.
         */
        int64_t long_value;

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
    } field_copy; /**< \brief Copy of the parsed field value. Which member is
                       valid is given by jkii_field_t#type. */

    /** Length of field_copy#string. ignored if field_copy#string is
     * null or jkii_field_t#type is not
     * jkii_field_type_t#JKII_FIELD_TYPE_STRING,
     * jkii_field_type_t#JKII_FIELD_TYPE_OBJECT and
     * jkii_field_type_t#JKII_FIELD_TYPE_ARRAY.
     */
    size_t field_copy_buff_size;

} jkii_field_t;

/** Parse JSON string.
 *
 * The string has to be one JSON object or array, after whitespace if any,
 * and before whitespace if any: valid JSON, as ECMA-404 has it, which is
 * checked whole, nesting objects and arrays at most 32 levels deep. Nothing
 * is allocated, and the string is not changed. Anything else is
 * jkii_parse_err_t#JKII_ERR_INVALID_INPUT, and each field's
 * jkii_field_t#result jkii_field_err_t#JKII_FIELD_ERR_NOT_FOUND.
 *
 *  \param [in] json_string of JSON string.
 *  \param [in] json_string_len of JSON string.
 *  \param [in,out] fields of kii JSON parser.
 *  \return parse result.
 */
jkii_parse_err_t jkii_parse(
        const char* json_string,
        size_t json_string_len,
        jkii_field_t* fields);

/** A member of an object, or an element of an array, as jkii_next_member()
 * gives it. Its pointers are into the string given to jkii_next_member().
 */
typedef struct jkii_member_t {
    /** The member's name, as written between its quotes, escapes and all,
     * which jkii_unescape_str() undoes, or NULL for an array's element. */
    const char* key;
    /** Length of jkii_member_t#key. */
    size_t key_length;
    /** The value: a string's characters between its quotes, as written,
     * escapes and all, as for jkii_member_t#key, and anything else whole, an
     * object or an array with its brackets. */
    const char* value;
    /** Length of jkii_member_t#value. */
    size_t value_length;
    /** The value's type, a number's as jkii_field_t#type has it: an integer's
     * follows its value. */
    jkii_field_type_t type;
} jkii_member_t;

/** Where jkii_next_member() has got to in an object or an array. Zeroed, it
 * starts at the first member.
 */
typedef struct jkii_member_iter_t {
    size_t _start; /**< \private */
    size_t _next; /**< \private */
} jkii_member_iter_t;

/** Gives the next member of the object, or element of the array, that a JSON
 * string is.
 *
 * The first call, with iter zeroed, checks the string as jkii_parse() does,
 * so it has to stay as it is until the last.
 *
 * \param [in] json_string The object or the array.
 * \param [in] json_string_len Its length.
 * \param [in,out] iter Where the last call got to, zeroed for the first.
 * \param [out] out_member The member, set only when JKII_ERR_OK is returned.
 * \return JKII_ERR_OK with the next member, JKII_ERR_NOT_FOUND when there
 * is none left, JKII_ERR_INVALID_INPUT if the string is not JSON, or
 * JKII_ERR_ROOT_TYPE if it is neither an object nor an array.
 */
jkii_parse_err_t jkii_next_member(
        const char* json_string,
        size_t json_string_len,
        jkii_member_iter_t* iter,
        jkii_member_t* out_member);

/** Gives the value a JSON string is, of any type, as jkii_next_member() gives
 * a member.
 *
 * The string is checked as jkii_parse() does, but may be any value, not only
 * an object or an array: a string, a number, true, false or null too. The
 * value comes without the whitespace around it, as jkii_member_t has it, and
 * without a key. An object or an array can then be walked with
 * jkii_next_member() on jkii_member_t#value.
 *
 * \param [in] json_string The value.
 * \param [in] json_string_len Its length.
 * \param [out] out_value The value, with jkii_member_t#key NULL, set only when
 * JKII_ERR_OK is returned.
 * \return JKII_ERR_OK, or JKII_ERR_INVALID_INPUT if the string is not JSON.
 */
jkii_parse_err_t jkii_root_value(
        const char* json_string,
        size_t json_string_len,
        jkii_member_t* out_value);

/** Gives the value a path leads to in a JSON string, as jkii_next_member()
 * gives a member: one value, where jkii_parse() reads several.
 * The string is checked as jkii_parse() does. The path is as
 * jkii_field_t#path has it: "/name" for a member of the root, "/a/b/[0]"
 * through objects and arrays, with `\/`, `\[`, `\]` and `\\` for those
 * characters in a name. A name is compared as the document writes it, escapes
 * and all, and an object's first member of that name is taken, as
 * jkii_parse() takes it. The value comes with jkii_member_t#key NULL.
 * \param [in] json_string The document.
 * \param [in] json_string_len Its length.
 * \param [in] path The path, null-terminated.
 * \param [out] out_member The value, set only when JKII_ERR_OK is returned.
 * \return JKII_ERR_OK, JKII_ERR_NOT_FOUND if the path leads to nothing or
 * isn't one, JKII_ERR_ROOT_TYPE if the root is neither an object nor an
 * array, or JKII_ERR_INVALID_INPUT if the string is not JSON.
 */
jkii_parse_err_t jkii_find_member(
        const char* json_string,
        size_t json_string_len,
        const char* path,
        jkii_member_t* out_member);

/** Copies a member's string, unescaped as jkii_unescape_str() does, into a
 * buffer, null-terminated.
 * \param [in] member The member, as jkii_next_member(), jkii_root_value() or
 * jkii_find_member() gives it.
 * \param [out] buff The string. On failure, an empty string, if buff_size
 * isn't 0.
 * \param [in] buff_size Its size, the terminator included: the member's
 * jkii_member_t#value_length + 1 always suffices.
 * \return JKII_FIELD_ERR_OK; JKII_FIELD_ERR_TYPE_MISMATCH if the member isn't
 * a string, null included; or JKII_FIELD_ERR_COPY if it doesn't fit, or holds
 * `\u0000`, which a C string can't.
 */
jkii_field_err_t jkii_member_string(
        const jkii_member_t* member,
        char* buff,
        size_t buff_size);

/** Reads a member's number as an integer within [min, max].
 * The number's value, read exactly from its digits, without rounding: an
 * integer as written, or a number with a fraction or an exponent whose value
 * is whole, as 30.0, 3e1 or 300e-1 for 30, or 1e18 for 10^18. One that isn't
 * whole, as 30.5 or 1e-1, is a type mismatch, as is anything but a number,
 * null included. Numbers in strings, as "30", are left to the caller.
 * \param [in] member The member, as jkii_next_member(), jkii_root_value() or
 * jkii_find_member() gives it.
 * \param [in] min The least it may be.
 * \param [in] max The most it may be.
 * \param [out] out_value The integer, set only when JKII_FIELD_ERR_OK is
 * returned.
 * \return JKII_FIELD_ERR_OK; JKII_FIELD_ERR_TYPE_MISMATCH; or
 * JKII_FIELD_ERR_NUM_OVERFLOW if it is above max, or
 * JKII_FIELD_ERR_NUM_UNDERFLOW below min, int64_t's range included.
 */
jkii_field_err_t jkii_member_integer(
        const jkii_member_t* member,
        int64_t min,
        int64_t max,
        int64_t* out_value);

/** Reads a member's number as a double.
 * A number of any form, an integer, a fraction or one with an exponent, as
 * jkii_parse() reads a double, to the nearest double. Anything but a number,
 * null and a number in a string included, is a type mismatch.
 * \param [in] member The member, as jkii_next_member(), jkii_root_value() or
 * jkii_find_member() gives it.
 * \param [out] out_value The double, never NaN nor an infinity, set only when
 * JKII_FIELD_ERR_OK is returned.
 * \return JKII_FIELD_ERR_OK; JKII_FIELD_ERR_TYPE_MISMATCH; or, as jkii_parse()
 * reports them, JKII_FIELD_ERR_NUM_OVERFLOW for a magnitude past a double's,
 * and JKII_FIELD_ERR_NUM_UNDERFLOW for one too small for it.
 */
jkii_field_err_t jkii_member_double(
        const jkii_member_t* member,
        double* out_value);

/** Reads a member's true or false.
 * \param [in] member The member, as jkii_next_member(), jkii_root_value() or
 * jkii_find_member() gives it.
 * \param [out] out_value JKII_TRUE or JKII_FALSE, set only when
 * JKII_FIELD_ERR_OK is returned.
 * \return JKII_FIELD_ERR_OK, or JKII_FIELD_ERR_TYPE_MISMATCH for anything
 * else, null and "true" as a string included.
 */
jkii_field_err_t jkii_member_boolean(
        const jkii_member_t* member,
        jkii_boolean_t* out_value);

/** A string holding a number, true or false, as that value, for the readers.
 * For JSON that sends a number or a boolean in a string: "30", "-2.5", "3e1",
 * "true". The readers take only their own JSON type: through this, a caller
 * takes such a string too, as in
 * jkii_member_integer(jkii_member_unquote(&m, &u), 1, 3600, &value).
 *
 * A string is taken only if its characters, as written between its quotes,
 * are exactly a JSON number, true or false, as ECMA-404 has them. So " 30",
 * "30 ", "+30", "030", ".5", "0x1E", "NaN", "Infinity", "TRUE", "null" and ""
 * are not, nor is a string with an escape, `"\u0033\u0030"` included. A string
 * taken reads as the value would: "30.5" is no integer, and "1e400" is
 * JKII_FIELD_ERR_NUM_OVERFLOW as a double.
 *
 * \param [in] member The member, as jkii_next_member(), jkii_root_value() or
 * jkii_find_member() gives it.
 * \param [out] unquoted Set, if member is such a string, to the member with
 * the type of what the string holds, as the value would have it; with the
 * same key and the same characters.
 * \return unquoted, if member is such a string; member itself otherwise, a
 * string or not, for the readers to read or refuse as it is.
 */
const jkii_member_t* jkii_member_unquote(
        const jkii_member_t* member,
        jkii_member_t* unquoted);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif
