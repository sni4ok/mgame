ADD_EXECUTABLE(bybit bybit.cpp config.cpp parse.cpp)
TARGET_LINK_LIBRARIES(bybit exports lws)
