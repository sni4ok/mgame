PROJECT(evie)
ADD_LIBRARY(evie STATIC mlog.cpp mfile.cpp cvt.cpp utils.cpp socket.cpp
    thread.cpp config.cpp profiler.cpp mstring.cpp decimal.cpp)
TARGET_LINK_LIBRARIES(evie pthread)

