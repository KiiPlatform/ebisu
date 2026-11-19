#ifndef _EBISU_CLOCK_IMPL
#define _EBISU_CLOCK_IMPL

#include "ebisu_version.h"

/* The time nothing built from this source runs earlier than: when it was
   committed, less a week for a committer whose clock was a little ahead. The
   floor khc_init() sets khc too. */
#define _EBISU_CLOCK_FLOOR (EBISU_SOURCE_TIME - 7 * 86400)

#endif /* _EBISU_CLOCK_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
