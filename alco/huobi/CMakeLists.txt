INCLUDE_DIRECTORIES(../../)

SET(LIBWEBSOCKET_PATH ~/develop/libwebsockets/build/lib)
INCLUDE_DIRECTORIES(~/develop/libwebsockets/build/include)

find_library(websockets name websockets PATHS ${LIBWEBSOCKET_PATH})

ADD_EXECUTABLE(huobi huobi.cpp config.cpp parse.cpp)
TARGET_LINK_LIBRARIES(huobi evie exports ${websockets} z)
