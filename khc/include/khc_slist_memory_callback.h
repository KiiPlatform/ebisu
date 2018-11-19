/** \file khc_slist_memory_callback.h
 * Provides memory management abstraction for khc_slist.
 */
#ifndef _KHC_SLIST_MEMORY_CALLBACK
#define _KHC_SLIST_MEMORY_CALLBACK

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Callback function of khc_slist memory allocation.
 *
 * @param [in] size size of allocated.
 * @param [in, out] userdata context data.
 * @returns void* allocated memory pointer
 */
typedef void*
    (*KHC_SLIST_CB_ALLOC)
    (size_t size, void* userdata);

/** \brief Callback function of khc_slist memory free.
 *
 * @param [in] ptr target pointer.
 * @param [in, out] userdata context data.
 */
typedef void
    (*KHC_SLIST_CB_FREE)
    (void* ptr, void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* _KHC_SLIST_MEMORY_CALBACK */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
