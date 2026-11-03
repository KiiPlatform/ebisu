/* jkii reads what Kii Cloud and Thing-IF send, and what the application passes
   to be sent to them, all of which is JSON: the strict grammar, so that what
   is not is refused rather than read as something else. */
#define JSMN_STRICT
#include "jsmn.h"
