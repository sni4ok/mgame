SET(LIBWEBSOCKET_PATH ~/develop/libwebsockets/build/lib)
INCLUDE_DIRECTORIES(~/develop/libwebsockets/build/include)

find_library(websockets name websockets PATHS ${LIBWEBSOCKET_PATH})

ADD_LIBRARY(lws STATIC lws.cpp alco.cpp)
TARGET_LINK_LIBRARIES(lws evie ${websockets})

SUBDIRS(bitfinex huobi binance coinbase kraken bitmex crypto.com)

IF("${MAKE_PLAZA}")
    SUBDIRS(plaza2)
ENDIF("${MAKE_PLAZA}")

