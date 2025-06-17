# this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info

add_library(${PROJECT_NAME}-analysis ${BLM_LIB_TYPE}
        loops/loop_analysis.cpp
        loops/loop_detector.cpp
        laa.cpp
)
