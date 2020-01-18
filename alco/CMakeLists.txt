INCLUDE_DIRECTORIES(../)

SET(LIBWEBSOCKET_PATH ~/develop/libwebsockets/build/lib)
INCLUDE_DIRECTORIES(~/develop/libwebsockets/build/include)

find_library(websockets name websockets PATHS ${LIBWEBSOCKET_PATH})

ADD_LIBRARY(lws STATIC lws.cpp)
TARGET_LINK_LIBRARIES(lws evie ${websockets})

SUBDIRS(bitfinex plaza2 huobi binance)

