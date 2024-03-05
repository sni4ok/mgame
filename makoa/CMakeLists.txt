ADD_LIBRARY(exports STATIC exports.cpp mmap.cpp)
TARGET_LINK_LIBRARIES(exports evie tyra dl)

ADD_LIBRARY(imports STATIC imports.cpp ../viktor/ifile.cpp)
TARGET_LINK_LIBRARIES(imports evie z)

ADD_LIBRARY(makoa SHARED engine.cpp server.cpp)
TARGET_LINK_LIBRARIES(makoa exports imports pthread)

ADD_EXECUTABLE(makoa_server makoa.cpp config.cpp)
TARGET_LINK_LIBRARIES(makoa_server makoa)

