PROJECT(viktor)
ADD_LIBRARY(viktor SHARED viktor.cpp)
ADD_LIBRARY(file SHARED efile.cpp)
ADD_LIBRARY(stat SHARED estat.cpp)
ADD_LIBRARY(stat_ticker SHARED stat_ticker.cpp)

IF("${MAKE_SQL}")
    ADD_LIBRARY(mysql SHARED mysql.cpp)
    TARGET_LINK_LIBRARIES(mysql evie mysqlclient)
ENDIF("${MAKE_SQL}")

TARGET_LINK_LIBRARIES(viktor exports)
TARGET_LINK_LIBRARIES(file exports)
TARGET_LINK_LIBRARIES(stat exports)
TARGET_LINK_LIBRARIES(stat_ticker exports)

ADD_EXECUTABLE(pip pip.cpp)
TARGET_LINK_LIBRARIES(pip makoa)

ADD_EXECUTABLE(utils utils.cpp)
TARGET_LINK_LIBRARIES(utils evie)

