/**
 * @file ebisu_mem.h
 * @brief Clearing memory that held a secret.
 */
#ifndef _EBISU_MEM
#define _EBISU_MEM

#include <stddef.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what the shared object exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Sets n bytes at p to 0, which the compiler can't leave out.
 *
 * A memset() just before memory is freed, or goes out of scope, is a store
 * nothing reads afterwards, which an optimizing compiler may remove, leaving
 * what was there. ebisu clears with this what held a password, a token or MQTT
 * credentials, once it is done with them. An application can do the same with
 * its own, such as a kii_t, which holds the thing's tokens, before its memory
 * is used for something else.
 *
 * \param [out] p The memory. May be NULL when n is 0.
 * \param [in] n How many bytes.
 */
void ebisu_secure_zero(void* p, size_t n);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_MEM */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
