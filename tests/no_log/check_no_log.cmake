# Looks for the ports' messages, and for writes to stderr, in an object built
# with logging, LOGGING, which has to have them, and in one built without,
# NO_LOG, which mustn't.
#
#   cmake -DLOGGING=<object> -DNO_LOG=<object> -P check_no_log.cmake

# What every port logs some of, and the stream it writes without a logger.
set(messages "failed to|rejected|timed out|resumed the TLS session")

file(STRINGS "${LOGGING}" found REGEX "${messages}")
if(NOT found)
  message(FATAL_ERROR "No message found in ${LOGGING}, which logs")
endif()
file(STRINGS "${NO_LOG}" found REGEX "${messages}")
if(found)
  message(FATAL_ERROR "${NO_LOG}, built without logging, has messages: ${found}")
endif()
file(STRINGS "${NO_LOG}" found REGEX "stderr|vfprintf|__iob_func|__acrt_iob_func")
if(found)
  message(FATAL_ERROR "${NO_LOG}, built without logging, writes to stderr: ${found}")
endif()
message(STATUS "${NO_LOG} has none of what ${LOGGING} logs")
