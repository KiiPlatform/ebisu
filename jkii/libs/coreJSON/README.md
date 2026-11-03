# coreJSON

[coreJSON](https://github.com/FreeRTOS/coreJSON) v3.3.1, the FreeRTOS project's
JSON parser, which jkii parses with. Its files are `source/core_json.c` and
`source/include/core_json.h` of that release, tag `v3.3.1`, unchanged, and its
`LICENSE`, MIT.

It is private to jkii: `jkii/src/jkii_core_json.c` includes `core_json.c`,
renaming its four functions so that they don't meet an application's own
coreJSON, and `libebisu_jkii` exports none of it. Its header is not installed,
and `core_json.c` is not built on its own: a build that takes every `.c` file
it finds leaves this folder out.

To update it, replace the three files with those of a newer release, and say
which in this file and in the repository's `NOTICE`. If the release adds a
function, or renames one, `jkii/src/jkii_core_json.h` has to rename it too,
which jkii's coexistence test checks.
