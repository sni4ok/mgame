PROJECT(evie)
ADD_LIBRARY(evie STATIC mlog.cpp mfile.cpp myitoa.cpp utils.cpp socket.cpp)
TARGET_LINK_LIBRARIES(evie pthread)

