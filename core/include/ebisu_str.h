/**
 * @file ebisu_str.h
 * @brief Bounded strings and numbers: copying, building, parsing and splitting.
 */
#ifndef _EBISU_STR
#define _EBISU_STR

#include <stdbool.h>
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

/**
 * \brief The most decimal places ebisu_strbuf_append_fixed() and
 * ebisu_parse_fixed() take.
 */
#define EBISU_FIXED_MAX_DECIMALS 18

/**
 * \brief Copies a string into a buffer of size bytes.
 *
 * Copies as much of src as fits with the terminating NUL, and terminates dst
 * whenever size is above 0, as strlcpy() does and strncpy() doesn't.
 *
 * \param [out] dst The buffer. Not touched when size is 0, so it may be NULL
 * then.
 * \param [in] src The string. NULL is taken as "".
 * \param [in] size Size of dst, in bytes.
 * \return The length of src, so the copy was cut short if the result is size
 * or more.
 */
size_t ebisu_strlcpy(char* dst, const char* src, size_t size);

/**
 * \brief A string built in a fixed buffer.
 *
 * The buffer always holds a terminated string: what the appends that fitted
 * appended, whole. An append that doesn't fit, with its terminating NUL,
 * appends nothing, and the builder fails: every later append does nothing, and
 * ebisu_strbuf_ok() is false. So appending a whole message and checking once
 * at the end is enough, and a failed message is never sent cut short.
 *
 * Initialize with ebisu_strbuf_init(). The fields are for reading only.
 */
typedef struct ebisu_strbuf_t {
    /** \brief The buffer. */
    char* buff;
    /** \brief Size of the buffer, in bytes. */
    size_t size;
    /** \brief Length of the string in the buffer, without its NUL. */
    size_t len;
    /** \brief Whether an append didn't fit. */
    bool overflow;
} ebisu_strbuf_t;

/**
 * \brief Starts a string in a buffer.
 *
 * \param [out] sb The builder.
 * \param [out] buff The buffer, set to "" when size is above 0. May be NULL
 * when size is 0; then any append of a byte or more fails.
 * \param [in] size Size of buff, in bytes.
 */
void ebisu_strbuf_init(ebisu_strbuf_t* sb, char* buff, size_t size);

/**
 * \brief Appends a string.
 *
 * \param [in,out] sb The builder.
 * \param [in] s The string. NULL is taken as "".
 */
void ebisu_strbuf_append(ebisu_strbuf_t* sb, const char* s);

/**
 * \brief Appends n bytes.
 *
 * \param [in,out] sb The builder.
 * \param [in] s The bytes, which need no NUL after them. May be NULL when n
 * is 0.
 * \param [in] n How many.
 */
void ebisu_strbuf_append_n(ebisu_strbuf_t* sb, const char* s, size_t n);

/**
 * \brief Appends one character.
 *
 * \param [in,out] sb The builder.
 * \param [in] c The character.
 */
void ebisu_strbuf_append_char(ebisu_strbuf_t* sb, char c);

/**
 * \brief Appends an integer in decimal, with a '-' when it is negative.
 *
 * \param [in,out] sb The builder.
 * \param [in] v The integer.
 */
void ebisu_strbuf_append_int(ebisu_strbuf_t* sb, int64_t v);

/**
 * \brief Appends an unsigned integer in decimal.
 *
 * \param [in,out] sb The builder.
 * \param [in] v The integer.
 */
void ebisu_strbuf_append_uint(ebisu_strbuf_t* sb, uint64_t v);

/**
 * \brief Appends an unsigned integer in lowercase hexadecimal, without "0x".
 *
 * \param [in,out] sb The builder.
 * \param [in] v The integer.
 * \param [in] min_digits The fewest digits to write, padding with '0': 0 or 1
 * write as many as v needs, and at most 16 are written.
 */
void ebisu_strbuf_append_hex(ebisu_strbuf_t* sb, uint64_t v, unsigned int min_digits);

/**
 * \brief Appends bytes in lowercase hexadecimal, two digits for each:
 * { 0x01, 0xab } is "01ab".
 *
 * \param [in,out] sb The builder.
 * \param [in] bytes The bytes. May be NULL when n is 0.
 * \param [in] n How many.
 */
void ebisu_strbuf_append_hex_bytes(ebisu_strbuf_t* sb, const uint8_t* bytes, size_t n);

/**
 * \brief Appends a scaled integer as a decimal number: (12345, 2) is
 * "123.45", (-5, 2) is "-0.05" and (7, 0) is "7".
 *
 * Without floating point, so the number is exact, and the same with every C
 * library and locale.
 *
 * \param [in,out] sb The builder.
 * \param [in] v The number times 10 to the power of decimals.
 * \param [in] decimals Decimal places, up to #EBISU_FIXED_MAX_DECIMALS. More
 * fail the builder.
 */
void ebisu_strbuf_append_fixed(ebisu_strbuf_t* sb, int64_t v, unsigned int decimals);

/**
 * \brief The room after the string, for a function that writes into a buffer
 * itself, such as an escaper or an encoder, to append there.
 *
 * What it writes is appended by ebisu_strbuf_commit(). Until then the builder
 * holds what it held, and the room can be asked for again.
 *
 * \param [in] sb The builder.
 * \param [out] out_size Size of the room, in bytes, with that of the
 * terminating NUL: what a function writing a terminated string is given.
 * \return Where the room starts, the string's NUL; or NULL, with a size of 0,
 * once the builder has failed, or for a buffer of size 0.
 */
char* ebisu_strbuf_room(ebisu_strbuf_t* sb, size_t* out_size);

/**
 * \brief Appends what was written in the room ebisu_strbuf_room() gave.
 *
 * n as large as the room or larger, which can't have fitted with a NUL after
 * it, appends nothing and fails the builder, as an append that doesn't fit
 * does: so a writer that ran out of room is committed with SIZE_MAX. Committing
 * 0 bytes never fails it. Either way the buffer is terminated again, after
 * what the appends that fitted appended, whatever the writer left in the room.
 * Does nothing once the builder has failed.
 *
 * \param [in,out] sb The builder.
 * \param [in] n How many bytes were written, without a NUL after them.
 */
void ebisu_strbuf_commit(ebisu_strbuf_t* sb, size_t n);

/**
 * \brief Whether every append so far fitted.
 *
 * \param [in] sb The builder.
 * \return false if one didn't, or the decimals given to
 * ebisu_strbuf_append_fixed() were too many.
 */
bool ebisu_strbuf_ok(const ebisu_strbuf_t* sb);

/** \brief Result of the ebisu_parse_ functions. */
typedef enum ebisu_parse_err_t {
    /** \brief Parsed. */
    EBISU_PARSE_OK,
    /** \brief There was nothing to parse. */
    EBISU_PARSE_EMPTY,
    /** \brief Something in it isn't part of the number. */
    EBISU_PARSE_INVALID,
    /** \brief A number, but beyond what the type or the bounds given take. */
    EBISU_PARSE_RANGE
} ebisu_parse_err_t;

/*
 * The ebisu_parse_ functions read len bytes at s, which need no NUL after
 * them, and never read beyond them. They are strict: all len bytes have to be
 * the number, with no spaces and no prefix, and leading zeros are allowed, as
 * fixed-width fields have them. They don't depend on the locale and don't set
 * errno. The result is written to *out only when they return EBISU_PARSE_OK.
 *
 * When a span both has a character that isn't part of a number and would be
 * out of range, the result is EBISU_PARSE_INVALID.
 */

/**
 * \brief Parses an unsigned decimal integer: digits only.
 *
 * \param [in] s The bytes.
 * \param [in] len How many.
 * \param [in] max The largest value taken.
 * \param [out] out The value.
 * \return #EBISU_PARSE_OK, or why not.
 */
ebisu_parse_err_t ebisu_parse_uint(const char* s, size_t len, uint64_t max, uint64_t* out);

/**
 * \brief Parses a decimal integer: digits, after a '+' or '-' if wanted.
 *
 * \param [in] s The bytes.
 * \param [in] len How many.
 * \param [in] min The smallest value taken.
 * \param [in] max The largest value taken.
 * \param [out] out The value.
 * \return #EBISU_PARSE_OK, or why not.
 */
ebisu_parse_err_t ebisu_parse_int(const char* s, size_t len, int64_t min, int64_t max, int64_t* out);

/**
 * \brief Parses an unsigned hexadecimal integer: digits and either case of
 * 'a' to 'f', without "0x".
 *
 * \param [in] s The bytes.
 * \param [in] len How many.
 * \param [in] max The largest value taken.
 * \param [out] out The value.
 * \return #EBISU_PARSE_OK, or why not.
 */
ebisu_parse_err_t ebisu_parse_hex(const char* s, size_t len, uint64_t max, uint64_t* out);

/**
 * \brief Parses hexadecimal into bytes, two digits for each, and either case
 * of 'a' to 'f': "01AB" is { 0x01, 0xab }.
 *
 * An odd number of digits is #EBISU_PARSE_INVALID, and more bytes than out
 * has room for are #EBISU_PARSE_RANGE.
 *
 * \param [in] s The bytes.
 * \param [in] len How many.
 * \param [out] out The bytes parsed. May be NULL when out_size is 0.
 * \param [in] out_size Size of out, in bytes.
 * \param [out] out_len How many bytes were parsed: len / 2.
 * \return #EBISU_PARSE_OK, or why not.
 */
ebisu_parse_err_t ebisu_parse_hex_bytes(const char* s, size_t len, uint8_t* out,
        size_t out_size, size_t* out_len);

/**
 * \brief Parses a decimal number into a scaled integer: with decimals 2,
 * "12.5" is 1250, "-0.05" is -5 and "7" is 700.
 *
 * A '+' or '-' if wanted, digits, and a '.' with digits after it if wanted.
 * There must be a digit before the '.' and after it, and no more digits after
 * it than decimals: one more would have to be rounded away, which is
 * #EBISU_PARSE_INVALID rather than a silent change of the value.
 *
 * \param [in] s The bytes.
 * \param [in] len How many.
 * \param [in] decimals Decimal places of the result, up to
 * #EBISU_FIXED_MAX_DECIMALS. More are #EBISU_PARSE_RANGE.
 * \param [in] min The smallest value taken, scaled.
 * \param [in] max The largest value taken, scaled.
 * \param [out] out The value, scaled.
 * \return #EBISU_PARSE_OK, or why not.
 */
ebisu_parse_err_t ebisu_parse_fixed(const char* s, size_t len, unsigned int decimals,
        int64_t min, int64_t max, int64_t* out);

/**
 * \brief len bytes at ptr: part of a string, or of data, without a copy.
 *
 * A span from ebisu_span_next() points into the span it was taken from, so it
 * is only good as long as what that points at is.
 */
typedef struct ebisu_span_t {
    /** \brief The first byte. NULL only for a span that was used up. */
    const char* ptr;
    /** \brief How many bytes. */
    size_t len;
} ebisu_span_t;

/**
 * \brief The span of a string, without its NUL.
 *
 * \param [in] s The string. NULL is taken as "".
 * \return Its span.
 */
ebisu_span_t ebisu_span_from_str(const char* s);

/**
 * \brief Takes the next field from a span, up to a delimiter or its end.
 *
 * Each call takes one field and advances *rest past it and its delimiter. Once
 * the field taken is the last, *rest is used up: its ptr is NULL, and the next
 * call returns false. So "a,b" gives "a" and "b", "a," gives "a" and "", and
 * "" gives one field, "". Check len before the first call for no fields from
 * an empty span.
 *
 * \param [in,out] rest What is left to split.
 * \param [in] delim The delimiter.
 * \param [out] field The field taken, without its delimiter.
 * \return true, or false if *rest was used up, leaving *field alone.
 */
bool ebisu_span_next(ebisu_span_t* rest, char delim, ebisu_span_t* field);

/**
 * \brief A span without the spaces, tabs, CRs and LFs at either end.
 *
 * \param [in] s The span.
 * \return What is between them, which may be empty.
 */
ebisu_span_t ebisu_span_trim(ebisu_span_t s);

/**
 * \brief Whether a span is exactly a string.
 *
 * \param [in] s The span.
 * \param [in] literal The string. NULL is taken as "".
 * \return true if both have the same bytes.
 */
bool ebisu_span_eq(ebisu_span_t s, const char* literal);

/**
 * \brief Whether a span is a string but for the case of ASCII letters, as
 * HTTP compares its header names and transfer codings.
 *
 * 'A' to 'Z' match 'a' to 'z', and no other bytes match but themselves: the
 * locale plays no part, as it does in tolower() and toupper().
 *
 * \param [in] s The span.
 * \param [in] literal The string. NULL is taken as "".
 * \return true if both have the same bytes, but for that.
 */
bool ebisu_span_eq_nocase(ebisu_span_t s, const char* literal);

/**
 * \brief Finds a string in a span, as strstr() does in a string, but within
 * the span's bytes alone: a NUL among them is a byte like any other, and none
 * is needed after them.
 *
 * \param [in] s The span.
 * \param [in] needle The string to find. NULL is taken as "", which is found
 * at 0.
 * \param [out] out_offset Where it starts in s, from the start of s: written
 * only when it is found.
 * \return Whether it was found.
 */
bool ebisu_span_find(ebisu_span_t s, const char* needle, size_t* out_offset);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_STR */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
