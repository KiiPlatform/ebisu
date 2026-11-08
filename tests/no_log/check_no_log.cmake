# Looks for MESSAGES, a regular expression, in the objects of a library built
# with logging, LOGGING, which have to have some of them, and in those of one
# built without, NO_LOG, which mustn't. Neither may write to stderr: a library
# logs only to the logger it is given. Each list of objects is separated by "|".
#
#   cmake -DMESSAGES=<regex> -DLOGGING=<objects> -DNO_LOG=<objects> \
#       -P check_no_log.cmake

string(REPLACE "|" ";" logging "${LOGGING}")
string(REPLACE "|" ";" no_log "${NO_LOG}")

set(writes_stderr "stderr|vfprintf|__iob_func|__acrt_iob_func")

set(logged "")
foreach(object IN LISTS logging)
  file(STRINGS "${object}" found REGEX "${MESSAGES}")
  list(APPEND logged ${found})
  file(STRINGS "${object}" found REGEX "${writes_stderr}")
  if(found)
    message(FATAL_ERROR "${object} writes to stderr: ${found}")
  endif()
endforeach()
if(NOT logged)
  message(FATAL_ERROR "No message found in ${LOGGING}, which logs")
endif()
foreach(object IN LISTS no_log)
  file(STRINGS "${object}" found REGEX "${MESSAGES}")
  if(found)
    message(FATAL_ERROR "${object}, built without logging, has messages: ${found}")
  endif()
  file(STRINGS "${object}" found REGEX "${writes_stderr}")
  if(found)
    message(FATAL_ERROR "${object}, built without logging, writes to stderr: ${found}")
  endif()
endforeach()
message(STATUS "${NO_LOG} has none of what ${LOGGING} logs")
