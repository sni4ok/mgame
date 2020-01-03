INCLUDE_DIRECTORIES(../../)
ADD_EXECUTABLE(plaza_templater plaza_templater.cpp)
ADD_EXECUTABLE(plaza plaza.cpp config.cpp parser.cpp)

SET(PLAZA_LIB_PATH ~/plaza/cgate-6.3.0.67/lib)
FIND_LIBRARY(plaza_cgate NAME libcgate.so PATHS ${PLAZA_LIB_PATH})
FIND_LIBRARY(plaza_sys NAME libP2Sys.so PATHS ${PLAZA_LIB_PATH})
FIND_LIBRARY(plaza_rep NAME libP2ReplClient.so PATHS ${PLAZA_LIB_PATH})

TARGET_LINK_LIBRARIES(plaza evie tyra ${plaza_cgate} ${plaza_sys} ${plaza_rep})
