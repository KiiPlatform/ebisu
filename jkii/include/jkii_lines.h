/**
 * @file jkii_lines.h
 * @brief Reading JSON Lines: one JSON value a line, fed in pieces of any size.
 *
 * JSON Lines, or NDJSON, keeps one JSON value on each line, ended with '\\n'. A
 * device can append a record without rewriting what came before, and read
 * them back one at a time, with a buffer the size of a record rather than of
 * the whole file or response. A raw '\\n' can't occur inside a JSON value, as
 * a string escapes it, so the lines are split without parsing them.
 *
 * \code
 * char line_buff[256];
 * jkii_lines_t r;
 * jkii_lines_init(&r, line_buff, sizeof(line_buff));
 * while ((n = read(fd, chunk, sizeof(chunk))) > 0) {
 *     const char* data = chunk;
 *     size_t left = n;
 *     while (left > 0) {
 *         size_t used = 0;
 *         const char* line = NULL;
 *         size_t line_len = 0;
 *         jkii_lines_err_t res = jkii_lines_feed(&r, data, left, &used, &line, &line_len);
 *         data += used;
 *         left -= used;
 *         if (res == JKII_LINES_RECORD) {
 *             // line_len bytes at line, terminated: give them to jkii_parse().
 *         }
 *     }
 * }
 * // And the last line, if the data didn't end with '\n'.
 * \endcode
 */
#ifndef JKII_LINES_H
#define JKII_LINES_H

#include <stddef.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what the shared object exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \brief What feeding a reader gives. */
typedef enum jkii_lines_err_t {
    /** \brief A record is whole: it is what the line points to. */
    JKII_LINES_RECORD,
    /** \brief What was fed is consumed, and no record is whole yet. */
    JKII_LINES_NEED_MORE,
    /** \brief A record longer than the buffer has ended. It is skipped, up to
     * the '\\n' ending it, and the reader goes on with the next. */
    JKII_LINES_TOO_LONG,
    /** \brief From jkii_lines_finish(): the data ended in the middle of a
     * record, with no '\\n' after it, as after a power loss while appending.
     * The line points to what there is of it, which may not parse. */
    JKII_LINES_TRUNCATED,
    /** \brief From jkii_lines_finish(): there is nothing left. */
    JKII_LINES_END
} jkii_lines_err_t;

/** \brief A reader. Set it up with jkii_lines_init(); its members are
 * private. */
typedef struct jkii_lines_t {
    char* _buff; /**< \private */
    size_t _buff_size; /**< \private */
    size_t _len; /**< \private */
    /* Skipping the rest of a record that didn't fit. */
    int _skipping; /**< \private */
    /* The buffer holds a record already returned, to drop at the next feed. */
    int _returned; /**< \private */
} jkii_lines_t;

/**
 * \brief Sets up a reader.
 *
 * \param [out] r The reader.
 * \param [out] buff Holds a record while it comes in, and once it is whole,
 * terminated. A record can be one byte shorter than it.
 * \param [in] buff_size Its size, at least 2.
 */
void jkii_lines_init(jkii_lines_t* r, char* buff, size_t buff_size);

/**
 * \brief Feeds a reader what has come, as it comes: a whole file, or a piece
 * of a response, of any size.
 *
 * It consumes data until a record ends, or to the end. Feed it again with
 * what is left, at data plus out_consumed. A record ends at '\\n', with a '\\r'
 * before it taken off too. Empty lines, and lines of whitespace only, aren't
 * records, and are skipped.
 *
 * \param [in, out] r The reader.
 * \param [in] data What has come.
 * \param [in] len Its length.
 * \param [out] out_consumed How much of data was consumed.
 * \param [out] out_line With JKII_LINES_RECORD, the record, in the buffer,
 * terminated. It stays there until the reader is next fed or finished.
 * \param [out] out_line_len Its length.
 * \return JKII_LINES_RECORD, JKII_LINES_NEED_MORE or JKII_LINES_TOO_LONG.
 */
jkii_lines_err_t jkii_lines_feed(jkii_lines_t* r, const char* data, size_t len,
        size_t* out_consumed, const char** out_line, size_t* out_line_len);

/**
 * \brief Ends the data, and gives what is left of it.
 *
 * \param [in, out] r The reader.
 * \param [out] out_line With JKII_LINES_TRUNCATED, what there is of the last
 * record, terminated.
 * \param [out] out_line_len Its length.
 * \return JKII_LINES_END, JKII_LINES_TRUNCATED, or JKII_LINES_TOO_LONG if the
 * data ended in a record longer than the buffer.
 */
jkii_lines_err_t jkii_lines_finish(jkii_lines_t* r, const char** out_line,
        size_t* out_line_len);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* JKII_LINES_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
