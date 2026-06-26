SET(LIBWEBSOCKET_PATH ~/develop/libwebsockets/build/lib)
INCLUDE_DIRECTORIES(~/develop/libwebsockets/build/include)

find_library(websockets name websockets PATHS ${LIBWEBSOCKET_PATH})

ADD_LIBRARY(lws STATIC lws.cpp alco.cpp)
TARGET_LINK_LIBRARIES(lws evie ${websockets})

function(add_parser name)

    SET(files ${name}/${name}.cpp ${name}/config.cpp ${name}/parse.cpp)
    ADD_EXECUTABLE(${name} ${files})
    ADD_LIBRARY(${name}_so SHARED ${files})
    TARGET_LINK_LIBRARIES(${name} exports lws)
    TARGET_LINK_LIBRARIES(${name}_so exports lws)
    set_target_properties(${name}_so PROPERTIES OUTPUT_NAME ${name})

endfunction()

set(parsers bitfinex huobi binance coinbase kraken bitmex crypto.com bybit)

foreach(parser IN LISTS parsers)
    add_parser(${parser})
endforeach()

IF("${MAKE_PLAZA}")
    SUBDIRS(plaza2)
ENDIF("${MAKE_PLAZA}")

