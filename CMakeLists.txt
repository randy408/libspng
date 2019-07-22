cmake_minimum_required(VERSION 3.0)

project(libspng C)

set(SPNG_MAJOR 0)
set(SPNG_MINOR 4)
set(SPNG_REVISION 6)
set(SPNG_VERSION ${SPNG_MAJOR}.${SPNG_MINOR}.${SPNG_REVISION})

option(OPTIMIZE_FILTER "Optimize (de)filtering, requires SSSE3 on x86" ON)
option(SPNG_SHARED "Build shared lib" ON)
option(SPNG_STATIC "Build static lib" ON)

include(GNUInstallDirs)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

set(spng_SOURCES spng.c)

if(OPTIMIZE_FILTER)
    add_definitions( -DSPNG_OPTIMIZE_FILTER=1 )
endif()

if(SPNG_SHARED)
	add_library(spng SHARED ${spng_SOURCES})
endif()

if(SPNG_STATIC)
	add_library(spng_static STATIC ${spng_SOURCES})
endif()

target_link_libraries(spng ${ZLIB_LIBRARIES})

install(TARGETS spng DESTINATION lib)
install(FILES spng.h DESTINATION include)


if(NOT CMAKE_HOST_WIN32 OR CYGWIN OR MINGW)
    set(prefix      ${CMAKE_INSTALL_PREFIX})
    set(exec_prefix ${CMAKE_INSTALL_PREFIX})
    set(libdir      ${CMAKE_INSTALL_FULL_LIBDIR})
    set(includedir  ${CMAKE_INSTALL_FULL_INCLUDEDIR})
    set(LIBS        "-lz -lm")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tests/libspng.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libspng.pc @ONLY)

	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tests/libspng.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()
