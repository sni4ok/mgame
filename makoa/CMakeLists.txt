ADD_LIBRARY(imports STATIC engine.cpp imports.cpp ../viktor/ifile.cpp)
TARGET_LINK_LIBRARIES(imports evie z dl)

ADD_LIBRARY(exports STATIC exports.cpp mmap.cpp)
TARGET_LINK_LIBRARIES(exports imports evie tyra)

ADD_LIBRARY(makoa SHARED server.cpp)
TARGET_LINK_LIBRARIES(makoa exports imports pthread)

ADD_EXECUTABLE(makoa_server makoa.cpp config.cpp)
TARGET_LINK_LIBRARIES(makoa_server makoa)

