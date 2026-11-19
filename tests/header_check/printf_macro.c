/* core's headers with printf defined as a macro, as a C library or an SDK that
   wraps it defines it: their format attributes still have to name printf's
   format, which the macro would turn into a name GCC doesn't know. */
#define printf ebisu_wrapped_printf

#include <ebisu_log.h>
#include <ebisu_str.h>

/* ISO C forbids an empty translation unit. */
typedef int ebisu_header_check_printf_macro;
