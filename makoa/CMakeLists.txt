ADD_LIBRARY(imports STATIC engine.cpp imports.cpp ../viktor/ifile.cpp)
TARGET_LINK_LIBRARIES(imports evie z dl)

ADD_LIBRARY(exports STATIC exports.cpp mmap.cpp)
TARGET_LINK_LIBRARIES(exports imports tyra)

ADD_LIBRARY(makoa SHARED server.cpp)
TARGET_LINK_LIBRARIES(makoa imports tyra exports)

ADD_EXECUTABLE(makoa_server makoa.cpp config.cpp)
TARGET_LINK_LIBRARIES(makoa_server makoa)

