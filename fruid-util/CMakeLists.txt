####################################################################################################################################
#
# CMakeLists.txt - CMake instructions for fruid-util.                                                                        [CMake]
#
# Copyright (c) 2019, Facebook Corporation. All rights reserved.
#
# License: Apache 2.0
#
####################################################################################################################################

cmake_minimum_required (VERSION 3.14.1 FATAL_ERROR)

# compiler selection
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
message( STATUS "Compiler in use is" ${CMAKE_CXX_COMPILER})

project( fruidutil VERSION 0.1 LANGUAGES CXX)

add_executable( fruid-util.bin source/fruid-util.cpp )
target_link_libraries( fruid-util.bin 
                          ncurses
                          sdbusplus
                          pthread
                          systemd
                          boost_program_options.a
                          ${Boost_LIBRARIES})

find_package (PkgConfig REQUIRED)
pkg_check_modules (SDBUSPLUSPLUS sdbusplus REQUIRED)

install( TARGETS fruid-util.bin)
