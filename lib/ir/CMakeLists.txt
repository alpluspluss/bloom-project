# this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info

add_library(${PROJECT_NAME}-ir ${BIR_LIB_TYPE}
        builder.cpp
        print.cpp
        tree-visual.cpp
)
