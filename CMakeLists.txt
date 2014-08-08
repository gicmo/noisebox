cmake_minimum_required (VERSION 2.8.10)
project (noisebox CXX)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)

set(VERSION_ABI   1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_INIT} -std=c++11") ## Optimize
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wunreachable-code")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -ggdb3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(ZMQ REQUIRED)
find_package(JsonCpp REQUIRED)
find_package(BerkeleyDB REQUIRED)
find_package(Libdaemon REQUIRED)

include_directories(${JSONCPP_INCLUDE_DIR}
                    ${ZMQ_INCLUDE_DIR}
                    ${LIBDAEMON_INCLUDE_DIRS})

# nb c++ lib
file(GLOB_RECURSE noisebox_SOURCES lib/*.cc)
file(GLOB_RECURSE noisebox_INCLUDES lib/*.h*)
include_directories(lib)

add_library(noisebox SHARED ${noisebox_INCLUDES} ${noisebox_SOURCES})
target_link_libraries(noisebox ${ZMQ_LIBRARY} ${JSONCPP_LIBRARY} ${LIBDAEMON_LIBRARIES})
set_target_properties(noisebox PROPERTIES
		              VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
		              SOVERSION ${VERSION_ABI})

# sensor daemon (hw interface)

set(sensord_SOURCES
    src/sensord.cc)
set(sensord_INCLUDES
    src/sensor.h)

add_executable(nb-sensord ${sensord_SOURCES})
target_link_libraries(nb-sensord noisebox ${ZMQ_LIBRARY} ${JSONCPP_LIBRARY})

set(stored_SOURCES src/stored.cc)
add_executable(nb-stored ${stored_SOURCES})
target_link_libraries(nb-stored noisebox ${ZMQ_LIBRARY} ${JSONCPP_LIBRARY} ${BDB_LIBRARIES})

set(htu21d_SOURCES tools/htu21d.cc)
add_executable(htu21d ${htu21d_SOURCES})
target_link_libraries(htu21d noisebox)

set(mcp9808_SOURCES tools/mcp9808.cc)
add_executable(mcp9808 ${mcp9808_SOURCES})
target_link_libraries(mcp9808 noisebox)

set(daemon_SOURCES tools/daemon_test.cc)
add_executable(daemon_test ${daemon_SOURCES})
target_link_libraries(daemon_test noisebox)

# install

install(TARGETS htu21d mcp9808 noisebox
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

install(TARGETS nb-sensord nb-stored
        RUNTIME DESTINATION sbin
)

install(FILES ${CMAKE_SOURCE_DIR}/tools/nb-graph.py ${CMAKE_SOURCE_DIR}/lcd/nb-lcd
        DESTINATION bin)