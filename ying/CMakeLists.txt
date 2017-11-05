INCLUDE_DIRECTORIES(../)
PROJECT(ying)
ADD_LIBRARY(ying SHARED ying.cpp mirror.cpp)
TARGET_LINK_LIBRARIES(ying evie ncurses)

