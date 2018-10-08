cmake_minimum_required(VERSION 3.0)

project(libspng C)

set(SPNG_MAJOR 0)
set(SPNG_MINOR 3)
set(SPNG_REVISION 1)
set(SPNG_VERSION ${SPNG_MAJOR}.${SPNG_MINOR}.${SPNG_REVISION})

option(SPNG_SHARED "Build shared lib" ON)
option(SPNG_STATIC "Build static lib" ON)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})

IF(SPNG_SHARED)
	add_library(spng SHARED common.c decode.c)
endif()

if(SPNG_STATIC)
	add_library(spng_static STATIC common.c decode.c)
endif()

target_link_libraries(spng ${ZLIB_LIBRARIES})

install(TARGETS spng DESTINATION lib)
install(FILES spng.h DESTINATION include)