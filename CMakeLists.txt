# this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info

cmake_minimum_required(VERSION 3.30)
project(bloom VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# build options
option(BLM_BUILD_EXAMPLES "Build examples" OFF)
option(BLM_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BLM_BUILD_TESTS "Build tests" ON)
option(BLM_BUILD_SHARED_LIB "Build shared library" OFF)

set(BLM_LIB_TYPE_STR "")
if (BLM_BUILD_SHARED_LIB)
    set(BLIR_LIB_TYPE SHARED)
    set(BLM_LIB_TYPE_STR "Dynamic")
else ()
    set(BLIR_LIB_TYPE STATIC)
    set(BLM_LIB_TYPE_STR "Static")
endif ()

# status messages
message(STATUS "Build Bloom Examples: ${BLM_BUILD_EXAMPLES}")
message(STATUS "Build Bloom Benchmarks: ${BLM_BUILD_BENCHMARKS}")
message(STATUS "Build Bloom Tests: ${BLM_BUILD_TESTS}")
message(STATUS "Build Bloom as: ${BLM_LIB_TYPE_STR}")

# include the config file; will set stuff like sanitizers, compiler flags, etc.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.cmake)

# include path; note: will be scoped to targets later
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib)


# benchmarks
if (BLM_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    include_directories(${benchmark_INCLUDE_DIRS})
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
endif ()

# examples
if (BLM_BUILD_EXAMPLES)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
endif ()

# tests
if (BLM_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    include(GoogleTest)

    set(BLM_TESTS ${PROJECT_NAME}-test)
    add_executable(${BLM_TESTS}
            # analysis tests
            tests/analysis/loops/loop_analysis.cpp
            tests/analysis/laa.cpp

            # foundation tests
            tests/foundation/context.cpp
            tests/foundation/dbinfo.cpp
            tests/foundation/module.cpp
            tests/foundation/node.cpp
            tests/foundation/region.cpp
            tests/foundation/type-registry.cpp
            tests/foundation/typed-data.cpp
            tests/foundation/analysis-pass.cpp
            tests/foundation/pass.cpp
            tests/foundation/pass-context.cpp
            tests/foundation/pass-manager.cpp

            # ipo tests
            tests/ipo/callgraph.cpp
            tests/ipo/dce.cpp
            tests/ipo/inlining.cpp
            tests/ipo/pass-infra.cpp
#            tests/ipo/sccp.cpp
            tests/ipo/specializer.cpp

            # ir stuff tests
            tests/ir/builder.cpp

            # support tests
            tests/support/allocator.cpp
            tests/support/bt.cpp
            tests/support/string-table.cpp

            # transform tests
            tests/transform/instcombine/instcombine.cpp
            tests/transform/vectorize/slp.cpp
            tests/transform/adce.cpp
            tests/transform/constfold.cpp
            tests/transform/cse.cpp
            tests/transform/dce.cpp
            tests/transform/dse.cpp
            tests/transform/pre.cpp
            tests/transform/reassociation.cpp
            tests/transform/sroa.cpp
    )

    target_compile_options(${BLM_TESTS} PRIVATE ${SANITIZER_FLAGS})

    target_link_libraries(${BLM_TESTS} PRIVATE
            ${PROJECT_NAME}
            GTest::GTest
            GTest::Main
    )

    target_link_options(${BLM_TESTS} PRIVATE ${SANITIZER_FLAGS})

    gtest_discover_tests(${BLM_TESTS})

    # manual testing for examination of the IR
    add_executable(${PROJECT_NAME}-dnt
            tests/__dnt/test_printer.cpp
    )

    target_compile_options(${PROJECT_NAME}-dnt PRIVATE ${SANITIZER_FLAGS})

    target_link_libraries(${PROJECT_NAME}-dnt PRIVATE ${PROJECT_NAME})
endif ()
