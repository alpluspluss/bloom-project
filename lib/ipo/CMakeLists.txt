# this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info

add_library(${PROJECT_NAME}-ipo ${BLM_LIB_TYPE}
        callgraph.cpp
        pass-context.cpp
        dce.cpp
        inlining.cpp
        pass-manager.cpp
        #        experimental/sccp.cpp
        specializer.cpp
)
