INCLUDE_DIRECTORIES(../)
PROJECT(viktor)
ADD_LIBRARY(viktor SHARED viktor.cpp)
ADD_LIBRARY(mysql SHARED mysql.cpp)
ADD_LIBRARY(file SHARED efile.cpp)
ADD_LIBRARY(stat SHARED estat.cpp)
ADD_LIBRARY(stat_ticker SHARED stat_ticker.cpp)

TARGET_LINK_LIBRARIES(viktor exports)
TARGET_LINK_LIBRARIES(mysql evie mysqlclient)
TARGET_LINK_LIBRARIES(file evie)
TARGET_LINK_LIBRARIES(stat evie)
TARGET_LINK_LIBRARIES(stat_ticker evie)

ADD_EXECUTABLE(pip pip.cpp)
TARGET_LINK_LIBRARIES(pip makoa)

ADD_EXECUTABLE(utils utils.cpp)
TARGET_LINK_LIBRARIES(utils evie)

