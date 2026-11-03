/* coreJSON, as jkii uses it: its functions under names of jkii's own, so that
 * an application with a coreJSON of its own, as FreeRTOS and AWS IoT
 * applications often have, links both. jkii_core_json.c compiles coreJSON with
 * them, and jkii.c calls them through this header. Private to jkii. */
#ifndef JKII_CORE_JSON_H
#define JKII_CORE_JSON_H

#define JSON_Validate jkii_JSON_Validate
#define JSON_SearchT jkii_JSON_SearchT
#define JSON_SearchConst jkii_JSON_SearchConst
#define JSON_Iterate jkii_JSON_Iterate

#include "coreJSON/core_json.h"

#endif /* JKII_CORE_JSON_H */
