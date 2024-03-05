ADD_EXECUTABLE(kraken kraken.cpp config.cpp parse.cpp)
TARGET_LINK_LIBRARIES(kraken exports lws)
