ADD_EXECUTABLE(bitfinex bitfinex.cpp config.cpp parse.cpp)
TARGET_LINK_LIBRARIES(bitfinex exports lws)
