/**
 * @file ebisu_server_clock.h
 * @brief A clock for a device without a real-time clock: the time servers
 * state.
 */
#ifndef _EBISU_SERVER_CLOCK
#define _EBISU_SERVER_CLOCK

#include "ebisu_clock.h"
#include "ebisu_system.h"

#include <stdbool.h>
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
 * \brief The time servers state, for a device without a real-time clock.
 *
 * A TLS stack needs the time to check a certificate's validity period, and a
 * device without a real-time clock has none until something tells it. Until
 * then the clock has a floor, a time it is known not to be earlier than: when
 * the library's source was built, or a time the application saved and set with
 * ebisu_server_clock_set_floor(). Once a server states its time in a response
 * over TLS to a kii_t the clock is set on with kii_set_clock(), or the
 * application gives one with ebisu_server_clock_set_time() or
 * ebisu_server_clock_override_time(), the clock counts on from it with the
 * system's monotonic clock.
 *
 * So until then, the clock gives a lower bound of the time only. The same goes
 * long after a time was given, 24 hours by default
 * (ebisu_server_clock_set_exact_for()): a monotonic clock drifts, so what it
 * counts is then a lower bound too. And if the monotonic clock goes back, the
 * time given becomes the floor, until the next. A certificate refused as not
 * valid yet while the clock had the time suggests the time fell behind: the
 * clock's doubt, which ebisu_clock_forget_time() calls, makes it a lower bound
 * until the next, so that the device connects again.
 *
 * base is the clock ebisu is given: the sockets' and kii_t's. Initialize with
 * ebisu_server_clock_init(). The functions are safe to call from the socket
 * callbacks, and from any task once ebisu_server_clock_set_lock() has set a
 * lock.
 */
typedef struct ebisu_server_clock_t {
    ebisu_clock_t base; /**< \brief What ebisu is given. */
    /** \private Seconds since the epoch the time is known not to be earlier
     * than. */
    int64_t _floor;
    /** \private Whether _time has been set. */
    bool _time_known;
    /** \private A time given by a server or the application, in seconds since
     * the epoch. */
    int64_t _time;
    /** \private The monotonic time _time was given at. */
    uint64_t _time_at_ms;
    /** \private How long after it was given _time counts as exact. */
    uint64_t _exact_for_ms;
    /** \private Whose monotonic clock the time is counted on with. */
    ebisu_system_t* _system;
    /** \private The lock guarding the clock, or NULL. */
    ebisu_lock_t* _lock;
} ebisu_server_clock_t;

/**
 * \brief Initializes the clock, and makes base a clock of it.
 *
 * The clock starts with no time, and a floor shortly before the library's
 * source was built: EBISU_SOURCE_TIME, less a week for a committer whose clock
 * was a little ahead, as khc_init() sets for khc_get_resp_date().
 *
 * \param [out] clock The clock.
 * \param [in] system The system whose monotonic clock the time is counted on
 * with. It has to outlive the clock.
 * \return true; or false if the system has no monotonic clock, without which
 * the clock is initialized with its floor alone, and takes no time.
 */
bool ebisu_server_clock_init(ebisu_server_clock_t* clock, ebisu_system_t* system);

/**
 * \brief Set the lock guarding the clock.
 *
 * Needed when more than one task uses the clock, as when kii_t of different
 * tasks are set on it. The monotonic clock is called only with it held.
 *
 * The socket callbacks may use the clock while kii holds a session's lock, so
 * this has to be a lock of its own, not a session's.
 *
 * \param [out] clock The clock.
 * \param [in] lock The lock, or NULL for none. It has to outlive the clock.
 */
void ebisu_server_clock_set_lock(ebisu_server_clock_t* clock, ebisu_lock_t* lock);

/**
 * \brief Raise the time the clock is known not to be earlier than.
 *
 * A device can save the time from ebisu_clock_get_time() now and then, and set
 * it here when it starts again, so that the time it knows never goes back
 * further than that. A floor lower than the clock's is ignored.
 *
 * \param [out] clock The clock.
 * \param [in] floor Seconds since 1970-01-01T00:00:00Z.
 */
void ebisu_server_clock_set_floor(ebisu_server_clock_t* clock, int64_t floor);

/**
 * \brief Set how long a time given stays exact.
 *
 * For that long after it was given, the clock counts on from a time and gives
 * the result as the time. After that, as a lower bound only, allowing for a
 * monotonic clock running up to 1% fast: ebisu_clock_check_validity() then no
 * longer tells a validity period that has not started. A kii_t the clock is set
 * on gives it a time with every response, so this matters to a device that
 * makes no HTTP requests for a long time, only MQTT ones.
 *
 * \param [out] clock The clock.
 * \param [in] seconds 24 hours by default. 0 never takes a time as exact.
 */
void ebisu_server_clock_set_exact_for(ebisu_server_clock_t* clock, uint32_t seconds);

/**
 * \brief Give the clock the time, as a server would.
 *
 * kii gives the clock the date of each response over TLS to a kii_t it is set
 * on, as this does; this is for a time from a source of the application's own,
 * an NTP client for instance. A time earlier than the clock's floor is
 * ignored, as a server's is, and a later one taken, so it is worth as much as
 * the source: an NTP client that authenticates its servers, with NTS
 * (RFC 8915) or a symmetric key (RFC 8573), gives a time as good as a server's
 * over TLS, but plain NTP and SNTP are not authenticated, and anyone on the
 * way to their server can answer with any time. A clock set on a kii_t too
 * takes the time from both, whichever was given last.
 *
 * \param [out] clock The clock.
 * \param [in] now Seconds since 1970-01-01T00:00:00Z.
 * \return true if the time was taken; false if it was earlier than the floor,
 * or the system has no monotonic clock.
 */
bool ebisu_server_clock_set_time(ebisu_server_clock_t* clock, int64_t now);

/**
 * \brief Set the time, whatever the clock had.
 *
 * For a time the application knows to be right, from an operator or a source
 * it trusts: unlike ebisu_server_clock_set_time(), it is taken even if it is
 * earlier than the floor, which is lowered to it. It is then a time given like
 * any other: counted on, exact for as long as
 * ebisu_server_clock_set_exact_for() says, forgotten by a doubt, and replaced
 * by the next time a server states.
 *
 * \param [out] clock The clock.
 * \param [in] now Seconds since 1970-01-01T00:00:00Z.
 * \return true if the time was taken; false, leaving the clock as it was, if
 * the system has no monotonic clock.
 */
bool ebisu_server_clock_override_time(ebisu_server_clock_t* clock, int64_t now);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_SERVER_CLOCK */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
