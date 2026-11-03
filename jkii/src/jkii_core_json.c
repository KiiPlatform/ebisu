/* coreJSON's source, unchanged, compiled under the names jkii_core_json.h
 * gives its functions. Build this, not core_json.c itself, which would be a
 * second copy that nothing calls. coreJSON is under a folder of its own, found
 * through jkii/libs or beside this file, so that a build that takes every .c
 * file in a folder doesn't take it. */
#include "jkii_core_json.h"
#include "coreJSON/core_json.c"
