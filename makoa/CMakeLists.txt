INCLUDE_DIRECTORIES(../)

ADD_LIBRARY(exports STATIC exports.cpp mmap.cpp)
TARGET_LINK_LIBRARIES(exports evie tyra dl)

ADD_EXECUTABLE(makoa_server makoa.cpp engine.cpp server.cpp config.cpp imports.cpp ../viktor/ifile.cpp)
TARGET_LINK_LIBRARIES(makoa_server exports pthread)


