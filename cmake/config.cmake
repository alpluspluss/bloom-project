# this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info

set(COMMON_FLAGS "-Wall -Wextra -Wpedantic")
set(DEBUG_FLAGS "-O0 -g")
set(RELEASE_FLAGS "-O3")

option(BLM_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(BLM_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(BLM_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" ON)
option(BLM_ENABLE_LSAN "Enable LeakSanitizer" OFF)
option(BLM_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(BLM_USE_NATIVE_CPU "Enable -march=native" ON)
option(BLM_ENABLE_LTO "Enable Link Time Optimization" OFF)

if (BLM_ENABLE_ASAN AND BLM_ENABLE_TSAN)
    message(FATAL_ERROR "Cannot enable both AddressSanitizer and ThreadSanitizer at the same time.")
endif ()

set(SANITIZER_FLAGS "")

if (BLM_ENABLE_ASAN)
    list(APPEND SANITIZER_FLAGS -fsanitize=address)
endif ()

if (BLM_ENABLE_TSAN)
    list(APPEND SANITIZER_FLAGS -fsanitize=thread)
endif ()

if (BLM_ENABLE_UBSAN)
    list(APPEND SANITIZER_FLAGS -fsanitize=undefined)
endif ()

if (BLM_ENABLE_LSAN)
    if (NOT (APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64"))
        list(APPEND SANITIZER_FLAGS -fsanitize=leak)
    else ()
        message(WARNING "LeakSanitizer is unsupported on Apple Silicon; ignoring BLM_ENABLE_LSAN.")
    endif ()
endif ()

if (BLM_WARNINGS_AS_ERRORS)
    set(COMMON_FLAGS "${COMMON_FLAGS} -Werror")
endif ()

if (BLM_USE_NATIVE_CPU)
    set(ARCH_FLAGS "-march=native")
endif ()

set(FRAME_POINTER_FLAGS "-fno-omit-frame-pointer")

string(JOIN " " SANITIZER_FLAGS_STR ${SANITIZER_FLAGS})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} ${DEBUG_FLAGS} ${FRAME_POINTER_FLAGS} ${ARCH_FLAGS}")
    foreach (flag ${SANITIZER_FLAGS})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endforeach ()
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} ${RELEASE_FLAGS} ${ARCH_FLAGS}")
    set(SANITIZER_FLAGS "")
    set(SANITIZER_FLAGS_STR "")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} ${ARCH_FLAGS}")
endif ()

if (BLM_ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# status output
message(STATUS "Current Build Mode: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Sanitizers: ${SANITIZER_FLAGS_STR}")
message(STATUS "Warnings as errors: ${BLM_WARNINGS_AS_ERRORS}")
message(STATUS "LTO enabled: ${BLM_ENABLE_LTO}")
