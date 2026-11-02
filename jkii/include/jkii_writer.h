/**
 * @file jkii_writer.h
 * @brief Writing JSON, bounded, into a buffer or out through a callback.
 *
 * The writer puts in the commas, colons and quotes itself, escapes strings,
 * and refuses what wouldn't be JSON rather than writing it. It allocates
 * nothing and doesn't recurse: its state is the struct, which can live on a
 * small task's stack.
 *
 * \code
 * char body[128];
 * jkii_writer_t w;
 * size_t len = 0;
 * jkii_writer_init_buff(&w, body, sizeof(body));
 * jkii_write_object_start(&w);
 * jkii_write_key(&w, "power");
 * jkii_write_bool(&w, 1);
 * jkii_write_key(&w, "temperature");
 * jkii_write_double(&w, 21.5, 1);
 * jkii_write_object_end(&w);
 * if (jkii_writer_finish(&w, &len) == JKII_WRITE_OK) {
 *     // body is {"power":true,"temperature":21.5}, len 33.
 * }
 * \endcode
 *
 * Each call returns the first error so far, so checking the result of
 * jkii_writer_finish() alone is enough.
 *
 * It writes JSON Lines too, one value a line: jkii_write_record_end() after
 * each value ends its line, and the next value starts the next one. Through a
 * callback, a batch of records can go out as a request body, or be appended
 * to a file, without the batch being held whole.
 */
#ifndef JKII_WRITER_H
#define JKII_WRITER_H

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
 * \brief How deep objects and arrays may nest.
 *
 * Another depth changes the size of jkii_writer_t: define it alike for jkii
 * and for everything using it.
 */
#ifndef JKII_WRITER_MAX_DEPTH
#define JKII_WRITER_MAX_DEPTH 16
#endif

/** \brief Results of writing. */
typedef enum jkii_write_err_t {
    /** \brief Written. */
    JKII_WRITE_OK,
    /** \brief The output doesn't fit in the buffer. Writing stops, but goes
     * on counting, so that jkii_writer_finish() says how much would have
     * been needed. */
    JKII_WRITE_ERR_TOO_LARGE,
    /** \brief Not where it can go: a value where a key is expected, a key
     * where a value is, the end of a container that isn't open, a second
     * value at the top without jkii_write_record_end() between,
     * jkii_write_record_end() with no value whole before it, or
     * jkii_writer_finish() with containers still open or nothing written. */
    JKII_WRITE_ERR_STRUCTURE,
    /** \brief Objects and arrays nested deeper than
     * JKII_WRITER_MAX_DEPTH. */
    JKII_WRITE_ERR_TOO_DEEP,
    /** \brief A value JSON can't hold, or a NULL one: NaN, an infinity, a
     * double too large for its decimals, more than 9 decimals, or raw JSON
     * of no length. */
    JKII_WRITE_ERR_VALUE,
    /** \brief The write callback took less than it was given. */
    JKII_WRITE_ERR_CALLBACK
} jkii_write_err_t;

/**
 * \brief Takes the writer's output.
 *
 * \param [in] data What was written, not terminated.
 * \param [in] size Its length.
 * \param [in, out] userdata What jkii_writer_init_cb() was given.
 * \return How much of it was taken: less than size fails the writer with
 * JKII_WRITE_ERR_CALLBACK.
 */
typedef size_t (*JKII_CB_WRITE)(const char* data, size_t size, void* userdata);

/** \brief A writer. Set it up with jkii_writer_init_buff() or
 * jkii_writer_init_cb(); its members are private. */
typedef struct jkii_writer_t {
    char* _buff; /**< \private */
    size_t _buff_size; /**< \private */
    size_t _used; /**< \private */
    /* Everything written, whether or not it fitted. */
    size_t _total; /**< \private */
    JKII_CB_WRITE _cb; /**< \private */
    void* _userdata; /**< \private */
    jkii_write_err_t _err; /**< \private */
    size_t _depth; /**< \private */
    /* For each open container: whether it is an object, whether it has a
     * member yet, and whether a key is waiting for its value. */
    unsigned char _levels[JKII_WRITER_MAX_DEPTH]; /**< \private */
    /* Whether the value at the top has been written whole. */
    int _top_done; /**< \private */
    /* How many records jkii_write_record_end() has ended. */
    size_t _records; /**< \private */
} jkii_writer_t;

/**
 * \brief Sets up a writer into a buffer.
 *
 * Once jkii_writer_finish() has succeeded, the buffer holds the JSON,
 * terminated, so it has to be at least one byte longer. With buff NULL and
 * buff_size 0, nothing is written, and jkii_writer_finish() says how long
 * the JSON is: a dry run, to size a buffer.
 *
 * \param [out] w The writer.
 * \param [out] buff The buffer, or NULL for a dry run.
 * \param [in] buff_size Its size.
 */
void jkii_writer_init_buff(jkii_writer_t* w, char* buff, size_t buff_size);

/**
 * \brief Sets up a writer out through a callback.
 *
 * buff collects the output, and is given to cb whenever it is full, and at
 * jkii_writer_finish(), so the JSON can be longer than buff: a request body,
 * or a file. Any size from 1 works, and a larger one means fewer calls. The
 * output isn't terminated.
 *
 * \param [out] w The writer.
 * \param [out] buff The buffer the output is collected in.
 * \param [in] buff_size Its size, at least 1.
 * \param [in] cb Takes the output.
 * \param [in] userdata Given to cb.
 */
void jkii_writer_init_cb(jkii_writer_t* w, char* buff, size_t buff_size,
        JKII_CB_WRITE cb, void* userdata);

/** \brief Opens an object, as a value. \param [in, out] w The writer.
 * \return JKII_WRITE_OK or the first error so far. */
jkii_write_err_t jkii_write_object_start(jkii_writer_t* w);

/** \brief Closes the object open innermost. \param [in, out] w The writer.
 * \return JKII_WRITE_OK or the first error so far. */
jkii_write_err_t jkii_write_object_end(jkii_writer_t* w);

/** \brief Opens an array, as a value. \param [in, out] w The writer.
 * \return JKII_WRITE_OK or the first error so far. */
jkii_write_err_t jkii_write_array_start(jkii_writer_t* w);

/** \brief Closes the array open innermost. \param [in, out] w The writer.
 * \return JKII_WRITE_OK or the first error so far. */
jkii_write_err_t jkii_write_array_end(jkii_writer_t* w);

/**
 * \brief Writes the key of the next member of the object open innermost.
 *
 * \param [in, out] w The writer.
 * \param [in] key The key, terminated, escaped as jkii_escape_str() does.
 * \return JKII_WRITE_OK or the first error so far.
 */
jkii_write_err_t jkii_write_key(jkii_writer_t* w, const char* key);

/**
 * \brief Writes a string, escaped as jkii_escape_str() does.
 *
 * \param [in, out] w The writer.
 * \param [in] s The string, terminated.
 * \return JKII_WRITE_OK or the first error so far.
 */
jkii_write_err_t jkii_write_string(jkii_writer_t* w, const char* s);

/**
 * \brief Writes a string of a given length, which may hold null bytes.
 *
 * \param [in, out] w The writer.
 * \param [in] s The string.
 * \param [in] n Its length.
 * \return JKII_WRITE_OK or the first error so far.
 */
jkii_write_err_t jkii_write_string_n(jkii_writer_t* w, const char* s, size_t n);

/** \brief Writes a signed integer. \param [in, out] w The writer.
 * \param [in] v The value. \return JKII_WRITE_OK or the first error so far. */
jkii_write_err_t jkii_write_int(jkii_writer_t* w, int64_t v);

/** \brief Writes an unsigned integer. \param [in, out] w The writer.
 * \param [in] v The value. \return JKII_WRITE_OK or the first error so far. */
jkii_write_err_t jkii_write_uint(jkii_writer_t* w, uint64_t v);

/**
 * \brief Writes a number with a fixed number of decimals.
 *
 * Rounded to the nearest, half away from zero, with '.' whatever the locale,
 * and without printf, which some embedded C libraries build without floating
 * point. The rounding is done in double, so a value within the double's
 * precision of a half, such as 0.995 with 2 decimals, can come out one off in
 * the last decimal. A value that rounds to 0 is written without a sign.
 *
 * \param [in, out] w The writer.
 * \param [in] v The value. NaN and the infinities are JKII_WRITE_ERR_VALUE,
 * as is a value whose magnitude times 10 to the decimals is 2 to the 64 or
 * more.
 * \param [in] decimals Digits after the point, 0 to 9. With 0 there is no
 * point.
 * \return JKII_WRITE_OK or the first error so far.
 */
jkii_write_err_t jkii_write_double(jkii_writer_t* w, double v, int decimals);

/** \brief Writes true, for v other than 0, or false. \param [in, out] w The
 * writer. \param [in] v The value. \return JKII_WRITE_OK or the first error
 * so far. */
jkii_write_err_t jkii_write_bool(jkii_writer_t* w, int v);

/** \brief Writes null. \param [in, out] w The writer. \return JKII_WRITE_OK
 * or the first error so far. */
jkii_write_err_t jkii_write_null(jkii_writer_t* w);

/**
 * \brief Writes JSON that is already JSON, as a value.
 *
 * It is copied as it is, unchecked, with a comma before it where one is
 * needed: a whole value, such as a state the application keeps as JSON.
 *
 * \param [in, out] w The writer.
 * \param [in] json The JSON.
 * \param [in] n Its length, more than 0.
 * \return JKII_WRITE_OK or the first error so far.
 */
jkii_write_err_t jkii_write_raw(jkii_writer_t* w, const char* json, size_t n);

/**
 * \brief Ends a record of JSON Lines: the value at the top, which has to be
 * whole, is followed by '\\n', and the next value starts another record.
 *
 * Neither an array around the records nor commas between them are written.
 * jkii_writer_finish() then accepts what was written, whether or not the last
 * record was ended.
 *
 * A record written by a writer of its own, ended with this, is a line to
 * append to a file of JSON Lines. Written into a buffer, it can be appended
 * with one write, so that a write cut short leaves at most that line cut
 * short. If the file doesn't end with '\\n', as when its last write was cut
 * short, write one first: the new record then starts a line of its own,
 * rather than running on from what was cut short.
 *
 * \param [in, out] w The writer.
 * \return JKII_WRITE_OK or the first error so far: JKII_WRITE_ERR_STRUCTURE
 * if no value is whole at the top.
 */
jkii_write_err_t jkii_write_record_end(jkii_writer_t* w);

/**
 * \brief Ends the JSON.
 *
 * A writer into a buffer terminates it. A writer through a callback gives it
 * what is left in its buffer.
 *
 * \param [in, out] w The writer.
 * \param [out] out_len The length of the JSON, without the terminator. With
 * JKII_WRITE_ERR_TOO_LARGE, how long it would have been, so that a buffer of
 * that plus one holds it. May be NULL.
 * \return JKII_WRITE_OK, or the first error so far:
 * JKII_WRITE_ERR_STRUCTURE if a container is still open, or nothing was
 * written: neither a value nor a record.
 */
jkii_write_err_t jkii_writer_finish(jkii_writer_t* w, size_t* out_len);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* JKII_WRITER_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
