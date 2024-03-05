ADD_EXECUTABLE(coinbase coinbase.cpp config.cpp parse.cpp)
TARGET_LINK_LIBRARIES(coinbase exports lws)
