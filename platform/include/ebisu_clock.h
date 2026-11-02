/**
 * @file ebisu_clock.h
 * @brief The time of day, for a device without a real-time clock.
 */
#ifndef _EBISU_CLOCK
#define _EBISU_CLOCK

#include "ebisu_task.h"

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
 * \brief The time of day, for a device without a real-time clock.
 *
 * A TLS stack needs the time to check a certificate's validity period, and a
 * device without a real-time clock has none until something tells it. Until
 * then the clock has a floor, a time it is known not to be earlier than: when
 * the library's source was built, or a time the application saved and set with
 * ebisu_clock_set_floor(). Once a server states its time in a response to a
 * kii_t the clock is set on with kii_set_clock(), or the application sets one
 * with ebisu_clock_set_time(), the clock counts on from it with the monotonic
 * clock.
 *
 * So a check made before then has a lower bound of the time only: a certificate
 * that expired before it is known to have expired, but one that is not valid
 * until later cannot be told from one that is valid now. The same goes long
 * after a time was given, 24 hours by default (ebisu_clock_set_exact_for()): a
 * monotonic clock drifts, so what it counts is then a lower bound too. And if
 * the monotonic clock goes back, the time given becomes the floor, until the
 * next. A certificate refused as not valid yet while the clock had the time
 * suggests the time fell behind: ebisu_clock_forget_time() makes it a lower bound
 * until the next, so that the device connects again.
 *
 * ebisu_clock_check_validity() applies that, for socket callbacks to use when
 * checking the server's certificate, and ebisu_clock_get_time() gives the time
 * and which of the two it is, for a TLS stack to check with itself.
 *
 * Initialize with ebisu_clock_init(). The functions are safe to call from the
 * socket callbacks, and from any task once ebisu_clock_set_cb_lock() has set a
 * lock.
 */
typedef struct ebisu_clock_t {
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
    EBISU_CB_MONOTONIC_MS _cb_monotonic_ms; /**< \private */
    void* _monotonic_ms_data; /**< \private */
    EBISU_CB_LOCK _cb_lock; /**< \private */
    EBISU_CB_LOCK _cb_unlock; /**< \private */
    void* _lock_data; /**< \private */
} ebisu_clock_t;

/** \brief Whether a validity period covers the time, as ebisu_clock_t knows it. */
typedef enum ebisu_validity_t {
    /** \brief Valid, as far as the time is known: within the period when the
     * time is known, and not past its end when only a lower bound is. */
    EBISU_VALIDITY_OK,
    /** \brief Past the end of the period. */
    EBISU_VALIDITY_EXPIRED,
    /** \brief Before the start of the period, which is only told when the time
     * is known. */
    EBISU_VALIDITY_NOT_YET_VALID
} ebisu_validity_t;

/**
 * \brief Initializes a ebisu_clock_t.
 *
 * The clock starts with no time, and a floor shortly before the library's
 * source was built: EBISU_SOURCE_TIME, less a week for a committer whose clock
 * was a little ahead, as khc_init() sets for khc_get_resp_date().
 *
 * \param [out] clock ebisu_clock_t instance.
 * \param [in] cb_monotonic_ms The monotonic clock the time is counted on with.
 * \param [in] userdata Context object pointer passed to cb_monotonic_ms.
 */
void ebisu_clock_init(
        ebisu_clock_t* clock,
        EBISU_CB_MONOTONIC_MS cb_monotonic_ms,
        void* userdata);

/**
 * \brief Set the lock guarding the clock.
 *
 * Needed when more than one task uses the clock, as when kii_t of different
 * tasks are set on it. The monotonic clock is called only with it held.
 *
 * The socket callbacks may use the clock while kii holds a session's lock, so
 * this has to be a lock of its own, not a session's.
 *
 * \param [out] clock ebisu_clock_t instance.
 * \param [in] cb_lock Callback acquiring the lock.
 * \param [in] cb_unlock Callback releasing the lock.
 * \param [in] userdata Context object pointer passed to both.
 */
void ebisu_clock_set_cb_lock(
        ebisu_clock_t* clock,
        EBISU_CB_LOCK cb_lock,
        EBISU_CB_LOCK cb_unlock,
        void* userdata);

/**
 * \brief Raise the time the clock is known not to be earlier than.
 *
 * A device can save the time from ebisu_clock_get_time() now and then, and set it
 * here when it starts again, so that the time it knows never goes back further
 * than that. A floor lower than the clock's is ignored.
 *
 * \param [out] clock ebisu_clock_t instance.
 * \param [in] floor Seconds since 1970-01-01T00:00:00Z.
 */
void ebisu_clock_set_floor(ebisu_clock_t* clock, int64_t floor);

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
 * \param [out] clock ebisu_clock_t instance.
 * \param [in] seconds 24 hours by default. 0 never takes a time as exact.
 */
void ebisu_clock_set_exact_for(ebisu_clock_t* clock, uint32_t seconds);

/**
 * \brief Give the clock the time.
 *
 * kii does so with the date of each response to a kii_t the clock is set on;
 * this is for the time from anywhere else, a real-time clock or NTP. A time
 * earlier than the clock's floor is ignored.
 *
 * \param [out] clock ebisu_clock_t instance.
 * \param [in] now Seconds since 1970-01-01T00:00:00Z.
 * \return true if the time was taken, false if it was earlier than the
 * floor.
 */
bool ebisu_clock_set_time(ebisu_clock_t* clock, int64_t now);

/**
 * \brief Get the time.
 *
 * \param [in] clock ebisu_clock_t instance.
 * \param [out] out_now Seconds since 1970-01-01T00:00:00Z: the time, counted on
 * from the last one given; or a lower bound of it, which is the floor while no
 * time has been given, or while the floor is later, or the time counted on
 * less 1% once the time given is older than ebisu_clock_set_exact_for() allows.
 * \return true if out_now is the time, to within a few seconds; false
 * if it is a lower bound, a time that it is not earlier than.
 */
bool ebisu_clock_get_time(ebisu_clock_t* clock, int64_t* out_now);

/**
 * \brief Forget the time, keeping what was counted as the floor.
 *
 * For the socket callbacks to call when their TLS stack refuses the server's
 * certificate as not valid yet, while the clock had the time. On a device whose
 * monotonic clock stops while it sleeps, the time falls behind, and a server
 * that renewed its certificate meanwhile presents one that starts after it.
 * Without a time of the server's, the clock could not catch up: the connection
 * that would bring one is refused. Forgotten, the time becomes a lower bound,
 * which ebisu_clock_check_validity() and the socket callbacks do not refuse a
 * certificate for not having started by, so the next connection goes through,
 * and its response gives the clock the time again.
 *
 * What was counted on, less 1%, becomes the floor: the time is at least that,
 * however the monotonic clock ran.
 *
 * \param [out] clock ebisu_clock_t instance.
 */
void ebisu_clock_forget_time(ebisu_clock_t* clock);

/**
 * \brief Check a validity period, a certificate's for instance, against the
 * time.
 *
 * While the clock has the floor only, a period is valid unless it ended before
 * the floor: a period starting later cannot be told from one that has started.
 * khc_seconds_since_epoch() converts the dates a TLS stack gives.
 *
 * \param [in] clock ebisu_clock_t instance.
 * \param [in] not_before Start of the period, seconds since the epoch.
 * \param [in] not_after End of the period, inclusive, seconds since the epoch.
 * \return Whether the period covers the time, as far as it is known.
 */
ebisu_validity_t ebisu_clock_check_validity(
        ebisu_clock_t* clock,
        int64_t not_before,
        int64_t not_after);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_CLOCK */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
