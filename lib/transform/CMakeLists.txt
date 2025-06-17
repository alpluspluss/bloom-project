# this project is part of the Bloom project; licensed under the MIT license. see LICENSE for more info

add_library(${PROJECT_NAME}-transform ${BLM_LIB_TYPE}
        instcombine/instcombine.cpp
        vectorize/slp.cpp
        adce.cpp
        constfold.cpp
        cse.cpp
        dce.cpp
        dse.cpp
        pre.cpp
        reassociate.cpp
        sroa.cpp
)
