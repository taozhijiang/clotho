cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x " )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Woverloaded-virtual -Wpointer-arith -Wwrite-strings -Werror=format -march=native " )

add_definitions(-DNP_DEBUG)
include_directories( ../xtra_rhelz.x/include )

aux_source_directory(. DIR_LIB_SRCS)
add_library (tzkeeper STATIC ${DIR_LIB_SRCS})

add_subdirectory( test )
