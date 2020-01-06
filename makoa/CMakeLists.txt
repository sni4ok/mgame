INCLUDE_DIRECTORIES(../)

ADD_LIBRARY(exports STATIC exports.cpp)
TARGET_LINK_LIBRARIES(exports tyra dl)

ADD_EXECUTABLE(makoa_server makoa.cpp engine.cpp server.cpp config.cpp)
TARGET_LINK_LIBRARIES(makoa_server evie exports pthread)


