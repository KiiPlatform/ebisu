/**
 * @file ebisu_posix.h
 * @brief The system and a lock on POSIX.
 */
#ifndef _EBISU_POSIX
#define _EBISU_POSIX

#include <ebisu_system.h>

#include <pthread.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what a shared object built with it exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief The system on POSIX.
 *
 * Each task is a detached thread, which goes by itself when it ends. A delay is
 * nanosleep(), resumed through signals until the whole time has passed. The
 * monotonic clock is CLOCK_MONOTONIC. The system has no state, so this one
 * serves everything.
 *
 * \return The system.
 */
ebisu_system_t* ebisu_posix_system(void);

/** \brief A lock that is a pthread mutex. */
typedef struct ebisu_posix_lock_t {
    ebisu_lock_t base; /**< \brief What ebisu is given. */
    pthread_mutex_t mutex; /**< \private */
} ebisu_posix_lock_t;

/**
 * \brief Initializes the mutex, and makes base a lock of it.
 *
 * \param [out] lock The lock.
 * \return 0, or what pthread_mutex_init() returned on failure.
 */
int ebisu_posix_lock_init(ebisu_posix_lock_t* lock);

/**
 * \brief Destroys the mutex, which nothing may hold or use any more.
 *
 * \param [in,out] lock The lock.
 */
void ebisu_posix_lock_destroy(ebisu_posix_lock_t* lock);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_POSIX */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
