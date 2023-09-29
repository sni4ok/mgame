PROJECT(evie)
ADD_LIBRARY(evie STATIC mlog.cpp mfile.cpp myitoa.cpp utils.cpp socket.cpp math.cpp thread.cpp config.cpp profiler.cpp mstring.cpp)
TARGET_LINK_LIBRARIES(evie pthread)

