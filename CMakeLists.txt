cmake_minimum_required(VERSION 3.0)

project(libspng C)

set(SPNG_MAJOR 0)
set(SPNG_MINOR 4)
set(SPNG_REVISION 3)
set(SPNG_VERSION ${SPNG_MAJOR}.${SPNG_MINOR}.${SPNG_REVISION})

option(SPNG_SHARED "Build shared lib" ON)
option(SPNG_STATIC "Build static lib" ON)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})

set(spng_SOURCES src/common.c src/decode.c)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    add_definitions( -DSPNG_OPTIMIZE_DEFILTER=1 )
    set(CMAKE_C_FLAGS "-msse2")
    set(CMAKE_C_FLAGS "-mssse3")
    set(spng_SOURCES ${spng_SOURCES} src/filter_sse2_intrinsics.c)
endif()

if(SPNG_SHARED)
	add_library(spng SHARED ${spng_SOURCES})
endif()

if(SPNG_STATIC)
	add_library(spng_static STATIC ${spng_SOURCES})
endif()

target_link_libraries(spng ${ZLIB_LIBRARIES})

install(TARGETS spng DESTINATION lib)
install(FILES src/spng.h DESTINATION include)
