# this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/analysis)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/foundation)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ipo)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ir)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/support)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/transform)

add_library(${PROJECT_NAME} ${BIR_LIB_TYPE}
        lib.cpp
)

# link
target_link_libraries(${PROJECT_NAME} PUBLIC
        ${PROJECT_NAME}-analysis
        ${PROJECT_NAME}-foundation
        ${PROJECT_NAME}-ipo
        ${PROJECT_NAME}-ir
        ${PROJECT_NAME}-support
        ${PROJECT_NAME}-transform
)
