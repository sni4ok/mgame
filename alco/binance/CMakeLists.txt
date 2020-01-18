INCLUDE_DIRECTORIES(../../)

ADD_EXECUTABLE(binance binance.cpp config.cpp parse.cpp)
TARGET_LINK_LIBRARIES(binance exports lws)
