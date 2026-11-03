/**
 * @file ebisu_win32.h
 * @brief The system and a lock on Windows.
 */
#ifndef _EBISU_WIN32
#define _EBISU_WIN32

#include <ebisu_system.h>

#include <windows.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what a shared object built with it exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief The system on Windows.
 *
 * Each task runs on a thread of its own, made with _beginthreadex(), which
 * nothing waits for. A delay is Sleep(). The monotonic clock is
 * GetTickCount64(), which doesn't jump when the wall clock is set. The system
 * has no state, so this one serves everything.
 *
 * \return The system.
 */
ebisu_system_t* ebisu_win32_system(void);

/** \brief A lock that is an SRWLOCK, which needs no destroying. */
typedef struct ebisu_win32_lock_t {
    ebisu_lock_t base; /**< \brief What ebisu is given. */
    SRWLOCK srwlock; /**< \private */
} ebisu_win32_lock_t;

/**
 * \brief Initializes the SRWLOCK, and makes base a lock of it.
 *
 * \param [out] lock The lock.
 */
void ebisu_win32_lock_init(ebisu_win32_lock_t* lock);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_WIN32 */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
