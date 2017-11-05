INCLUDE_DIRECTORIES(../)
PROJECT(viktor)
ADD_LIBRARY(viktor SHARED viktor.cpp)

#viktor and tyra like sid and nancy or bonnie and clyde 
TARGET_LINK_LIBRARIES(viktor tyra evie)

