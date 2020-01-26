INCLUDE_DIRECTORIES(../../)

ADD_EXECUTABLE(bitmex bitmex.cpp config.cpp parse.cpp)
TARGET_LINK_LIBRARIES(bitmex exports lws)
