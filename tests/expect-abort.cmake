# Runs EXE CASE and requires abnormal termination with the invalid-handle report.
execute_process(COMMAND "${EXE}" "${CASE}" RESULT_VARIABLE result ERROR_VARIABLE error OUTPUT_QUIET)

if ("${result}" STREQUAL "0" OR "${result}" STREQUAL "2" OR "${result}" STREQUAL "7")
    message(FATAL_ERROR "${CASE}: expected termination, got exit code ${result}")
endif()
if (NOT error MATCHES "QuadTree: invalid handle passed to ")
    message(FATAL_ERROR "${CASE}: missing diagnostic, stderr: ${error}")
endif()
message(STATUS "${CASE}: terminated (${result}): ${error}")
