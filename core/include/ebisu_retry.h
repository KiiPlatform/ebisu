/**
 * @file ebisu_retry.h
 * @brief Waits before trying again after a failure, longer each time and
 * spread from device to device.
 */
#ifndef _EBISU_RETRY
#define _EBISU_RETRY

#include <stdint.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what the shared object exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A device that keeps at a cloud that is down or refusing it adds to the
 * trouble, and devices that lost the cloud together come back together. So
 * the wait after each failure in a row doubles up to a longest wait, and then
 * a device waits half to all of it, its share taken from a seed that differs
 * from device to device. The MQTT task waits so between its attempts to reach
 * the broker, with the thing's ID for the seed.
 *
 * The waits are in the caller's unit, milliseconds or seconds, and so is
 * what they come to. Nothing here has a source of randomness, a clock or
 * state: the caller keeps the count of failures, across restarts if it wants
 * to, and sleeps or schedules.
 *
 * Not "full jitter", from 0 to all of the wait: with it a wait can come out
 * near zero, which doesn't spare a cloud that is down.
 */

/**
 * \brief A seed for ebisu_retry_spread() and ebisu_retry_wait() from a
 * device's ID: its 32-bit FNV-1a hash.
 *
 * \param [in] id The ID, such as the thing's. NULL is taken as "".
 * \return The seed.
 */
uint32_t ebisu_retry_seed(const char* id);

/**
 * \brief A device's share of a wait: from half of it, rounded up, to all of
 * it.
 *
 * The share changes from wait to wait, so a device doesn't wait the same part
 * of every one.
 *
 * \param [in] wait The wait, in the caller's unit.
 * \param [in] seed The device's, from ebisu_retry_seed().
 * \return From wait - wait / 2 to wait: 0 for 0, 1 for 1.
 */
uint32_t ebisu_retry_spread(uint32_t wait, uint32_t seed);

/**
 * \brief The wait after the n-th failure in a row.
 *
 * first, doubled n - 1 times and max at most, then spread as
 * ebisu_retry_spread() spreads it. A max below first cuts the first wait short
 * too. Nothing overflows, for any n, first and max.
 *
 * \param [in] first The first wait, in the caller's unit.
 * \param [in] max The longest wait, in the same unit.
 * \param [in] n The failures in a row, from 1. 0 is taken as 1.
 * \param [in] seed The device's, from ebisu_retry_seed().
 * \return The wait, in the unit of first and max.
 */
uint32_t ebisu_retry_wait(uint32_t first, uint32_t max, uint32_t n, uint32_t seed);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_RETRY */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
