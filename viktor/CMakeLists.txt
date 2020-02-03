INCLUDE_DIRECTORIES(../)
PROJECT(viktor)
ADD_LIBRARY(viktor SHARED viktor.cpp)
ADD_LIBRARY(mysql SHARED mysql.cpp)
ADD_LIBRARY(file SHARED efile.cpp)
ADD_LIBRARY(stat SHARED estat.cpp)

TARGET_LINK_LIBRARIES(viktor tyra evie)
TARGET_LINK_LIBRARIES(file evie)
TARGET_LINK_LIBRARIES(stat evie)
TARGET_LINK_LIBRARIES(mysql evie mysqlclient)

ADD_EXECUTABLE(pip pip.cpp ifile.cpp ../makoa/imports.cpp)
TARGET_LINK_LIBRARIES(pip exports)

