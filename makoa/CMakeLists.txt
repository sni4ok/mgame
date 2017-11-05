INCLUDE_DIRECTORIES(../)
ADD_EXECUTABLE(makoa_server makoa.cpp engine.cpp server.cpp securities.cpp config.cpp exports.cpp)
TARGET_LINK_LIBRARIES(makoa_server evie pthread dl)

