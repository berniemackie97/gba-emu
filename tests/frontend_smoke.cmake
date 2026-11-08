if(NOT DEFINED PROGRAM OR NOT EXISTS "${PROGRAM}")
    message(FATAL_ERROR "Frontend executable is missing: ${PROGRAM}")
endif()

if(SCENARIO STREQUAL "startup" OR SCENARIO STREQUAL "invalid-arguments")
    set(driver "dummy")
elseif(SCENARIO STREQUAL "invalid-video")
    set(driver "gba-unknown-video-driver")
else()
    message(FATAL_ERROR "Unknown frontend smoke scenario: ${SCENARIO}")
endif()

if(SCENARIO STREQUAL "invalid-arguments")
    set(arguments "--unsupported")
else()
    set(arguments "--smoke-test")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "SDL_VIDEODRIVER=${driver}" "${PROGRAM}" ${arguments}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    TIMEOUT 15
)

if(SCENARIO STREQUAL "startup")
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Frontend startup failed (exit ${result}): ${errors}")
    endif()
elseif(SCENARIO STREQUAL "invalid-video")
    if(NOT "${result}" STREQUAL "1" OR NOT errors MATCHES "SDL_Init failed:")
        message(FATAL_ERROR "Expected initialization failure, got ${result}: ${errors}")
    endif()
elseif(SCENARIO STREQUAL "invalid-arguments")
    if(NOT "${result}" STREQUAL "2" OR NOT errors MATCHES "Usage: gba_sdl")
        message(FATAL_ERROR "Expected usage error, got ${result}: ${errors}")
    endif()
endif()
