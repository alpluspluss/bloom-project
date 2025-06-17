# this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info

add_library(${PROJECT_NAME}-foundation ${BLM_LIB_TYPE}
        analysis-pass.cpp
        context.cpp
        dbinfo.cpp
        module.cpp
        pass-context.cpp
        pass-manager.cpp
        region.cpp
        type-registry.cpp
        typed-data.cpp
)
