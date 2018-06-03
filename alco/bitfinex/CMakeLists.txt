INCLUDE_DIRECTORIES(../../)
ADD_EXECUTABLE(bitfinex bitfinex.cpp config.cpp parse.cpp)
TARGET_LINK_LIBRARIES(bitfinex evie viktor curl)
