#include "mbedtls_threading.h"

#include <FreeRTOS.h>
#include <semphr.h>

#include <mbedtls/threading.h>

/* A mutex that can't be made leaves the mutex NULL, which locking reports as
   bad input data rather than lock nothing. */
static void mutex_init(mbedtls_threading_mutex_t* mutex)
{
    mutex->mutex = xSemaphoreCreateMutex();
}

static void mutex_free(mbedtls_threading_mutex_t* mutex)
{
    if (mutex->mutex != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)mutex->mutex);
        mutex->mutex = NULL;
    }
}

static int mutex_lock(mbedtls_threading_mutex_t* mutex)
{
    if (mutex->mutex == NULL) {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    return xSemaphoreTake((SemaphoreHandle_t)mutex->mutex, portMAX_DELAY) == pdTRUE ?
        0 : MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
}

static int mutex_unlock(mbedtls_threading_mutex_t* mutex)
{
    if (mutex->mutex == NULL) {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    return xSemaphoreGive((SemaphoreHandle_t)mutex->mutex) == pdTRUE ?
        0 : MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
}

void mbedtls_threading_init_freertos(void)
{
    mbedtls_threading_set_alt(mutex_init, mutex_free, mutex_lock, mutex_unlock);
}
