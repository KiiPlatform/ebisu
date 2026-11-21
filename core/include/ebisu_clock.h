/**
 * @file ebisu_clock.h
 * @brief The time of day: what a clock does, and the checks made against one.
 */
#ifndef EBISU_CLOCK_H
#define EBISU_CLOCK_H

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

/** \brief A clock. See struct ebisu_clock_t. */
typedef struct ebisu_clock_t ebisu_clock_t;

/** \brief What a clock does.
 *
 * The socket callbacks call it as they check the server's certificate, and kii
 * as each response to a kii_t the clock is set on comes, from the task of each:
 * a clock with state guards it itself. See ebisu_object.h for how self leads to
 * that state.
 */
typedef struct ebisu_clock_ops_t {
    /**
     * \brief Gives the time.
     *
     * Required. A clock that doesn't have the time, as a device without a
     * real-time clock until something tells it, gives a lower bound of it: a
     * time it is known not to be earlier than.
     *
     * \param [in] self The clock.
     * \param [out] out_now Seconds since 1970-01-01T00:00:00Z: the time, or a
     * lower bound of it.
     * \return true if out_now is the time, to within a few seconds; false if
     * it is a lower bound.
     */
    bool (*now)(ebisu_clock_t* self, int64_t* out_now);

    /**
     * \brief Takes the time a server stated.
     *
     * Optional: NULL in a clock that keeps the time itself, as the system's
     * does. kii calls it with the Date of each response over TLS to a kii_t
     * the clock is set on, which khc takes only within the range a build
     * allows: see khc_get_resp_date(). It is the server's word, worth as much
     * as the check of its certificate.
     *
     * \param [in] self The clock.
     * \param [in] seconds Seconds since 1970-01-01T00:00:00Z.
     */
    void (*stated)(ebisu_clock_t* self, int64_t seconds);

    /**
     * \brief Doubts the time the clock gave.
     *
     * Optional: NULL in a clock that can't do better than the time it gave.
     * The socket callbacks call it, through ebisu_clock_forget_time(), when
     * their TLS stack refuses the server's certificate as not valid yet at a
     * time now gave as exact. More likely than a certificate from the future,
     * the clock fell behind: ebisu_server_clock_t then gives a lower bound
     * until a server states the time again, so that the next connection goes
     * through.
     *
     * \param [in] self The clock.
     */
    void (*doubt)(ebisu_clock_t* self);
} ebisu_clock_ops_t;

/**
 * \brief The time of day, which a TLS stack checks a certificate's validity
 * period against.
 *
 * A clock gives the time, or a lower bound of it when it doesn't have it.
 * Against a lower bound, a certificate that expired before it is known to have
 * expired, but one that is not valid until later cannot be told from one that
 * is valid now: ebisu_clock_check_validity() applies that.
 *
 * What gives the time is the clock's own:
 * - the system's: ebisu_system_clock(), which on POSIX and Windows gives the
 *   time of day the system has, and on FreeRTOS, which keeps none, the clock
 *   the application set on the system;
 * - for a device without a real-time clock, ebisu_server_clock_t: the time
 *   servers state over TLS, or a time source of the application's gives it,
 *   counted on with the system's monotonic clock;
 * - or the application's, over a board's real-time clock, or over a time
 *   source of its own, such as an NTP client that authenticates its servers,
 *   which embeds this in a struct of its own as ebisu_object.h shows, and may
 *   keep the time its source gives in an ebisu_server_clock_t it holds.
 *
 * Set it on the sockets, ebisu_openssl_socket_set_clock() and the like, and,
 * for a clock that takes the time from servers, on each kii_t with
 * kii_set_clock().
 */
struct ebisu_clock_t {
    const ebisu_clock_ops_t* ops; /**< \brief What the clock does. */
};

/** \brief Whether a validity period covers the time, as a clock knows it. */
typedef enum ebisu_validity_t {
    /** \brief Valid, as far as the time is known: within the period when the
     * clock has the time, and not past its end when it has a lower bound
     * only. */
    EBISU_VALIDITY_OK,
    /** \brief Past the end of the period. */
    EBISU_VALIDITY_EXPIRED,
    /** \brief Before the start of the period, which is only told when the
     * clock has the time. */
    EBISU_VALIDITY_NOT_YET_VALID
} ebisu_validity_t;

/**
 * \brief Get the time, with the clock's now.
 *
 * \param [in] clock The clock.
 * \param [out] out_now Seconds since 1970-01-01T00:00:00Z: the time, or a lower
 * bound of it.
 * \return true if out_now is the time, to within a few seconds; false if it is
 * a lower bound, a time that it is not earlier than.
 */
bool ebisu_clock_get_time(ebisu_clock_t* clock, int64_t* out_now);

/**
 * \brief Forget the time the clock gave, with its doubt, if it has one.
 *
 * For the socket callbacks to call when their TLS stack refuses the server's
 * certificate as not valid yet, at a time the clock gave as exact. On a device
 * whose monotonic clock stops while it sleeps, ebisu_server_clock_t falls
 * behind, and a server that renewed its certificate meanwhile presents one
 * that starts after it. Without a time of the server's, the clock could not
 * catch up: the connection that would bring one is refused. Forgotten, the time
 * becomes a lower bound, which ebisu_clock_check_validity() and the socket
 * callbacks do not refuse a certificate for not having started by, so the next
 * connection goes through, and its response gives the clock the time again.
 *
 * A clock without doubt, the system's, is left as it is.
 *
 * \param [in] clock The clock.
 */
void ebisu_clock_forget_time(ebisu_clock_t* clock);

/**
 * \brief Check a validity period, a certificate's for instance, against the
 * time.
 *
 * While the clock has a lower bound only, a period is valid unless it ended
 * before it: a period starting later cannot be told from one that has started.
 * ebisu_time_from_civil() converts the dates a TLS stack gives.
 *
 * \param [in] clock The clock.
 * \param [in] not_before Start of the period, seconds since the epoch.
 * \param [in] not_after End of the period, inclusive, seconds since the epoch.
 * \return Whether the period covers the time, as far as it is known.
 */
ebisu_validity_t ebisu_clock_check_validity(
        ebisu_clock_t* clock,
        int64_t not_before,
        int64_t not_after);

/**
 * \brief What a clock that reads a time of day kept elsewhere gives: the
 * system's, or a real-time clock's.
 *
 * A time earlier than shortly before the library's source was built is not
 * the time: nothing built from the source runs earlier. It is a time not set,
 * as a device without a real-time clock has at boot, 1970 or 2000, until NTP
 * sets it, or a real-time clock that lost its battery. Against it, every
 * certificate would be refused as not valid yet. So that one gives the floor
 * ebisu_server_clock_init() starts a clock with instead, as a lower bound.
 *
 * ebisu_system_clock() of the POSIX and Win32 ports gives its time this way,
 * and an application's clock over a real-time clock can too.
 *
 * \param [in] seconds The time of day read, seconds since
 * 1970-01-01T00:00:00Z.
 * \param [out] out_now What the clock gives: seconds, or the floor.
 * \return true if out_now is seconds, the time; false if it is the floor, a
 * lower bound.
 */
bool ebisu_clock_from_system_time(int64_t seconds, int64_t* out_now);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* EBISU_CLOCK_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
