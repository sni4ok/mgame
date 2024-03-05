ADD_EXECUTABLE(huobi huobi.cpp config.cpp parse.cpp)
TARGET_LINK_LIBRARIES(huobi exports lws z)
