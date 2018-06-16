PROJECT(evie)
ADD_LIBRARY(evie STATIC mlog.cpp mfile.cpp myitoa.cpp)
TARGET_LINK_LIBRARIES(evie pthread)
