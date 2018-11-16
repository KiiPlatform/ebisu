/** \file khc_memory_callback.h
 * Provides memory management abstraction.
 */
#ifndef _KHC_MEMORY_CALLBACK
#define _KHC_MEMORY_CALLBACK

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Callback function memory allocation.
 *
 * Applications must implement this callback in the target enviroment.
 *
 * \param [in] size size of allocated.
 * \param [in, out] userdata context data passed to khc_set_cb_mem_alloc(khc*, KHC_CB_MEM_ALLOC, void*)
 * \returns void* allocated memory pointer
 */
typedef void*
    (*KHC_CB_MEM_ALLOC)
    (size_t size, void* userdata);

/** \brief Callback function memory free.
 *
 * Applications must implement this callback in the target enviroment.
 *
 * \param [in] ptr target pointer.
 * \param [in, out] userdata context data passed to khc_set_cb_mem_free(khc*, KHC_CB_MEM_FREE, void*)
 */
typedef void
    (*KHC_CB_MEM_FREE)
    (void* ptr, void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* _KHC_MEMORY_CALBACK */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
