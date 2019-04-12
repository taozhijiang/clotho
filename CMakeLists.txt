cmake_minimum_required (VERSION 2.8.11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x " )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Woverloaded-virtual -Wpointer-arith -Wwrite-strings -Werror=format -march=native " )
include("${PROJECT_SOURCE_DIR}/help.cmake")
define_colors()

####################
### 修改编译选项 ###

# if want to see debug in verbose mode
# cmake -DCMAKE_VERBOSE_MAKEFILE=true ../
# if want to see build in detail
# make -- VERBOSE=1

option(BUILD_DEBUG "Build Project for Debug Propurse..." OFF)

if(BUILD_DEBUG)
    set(CMAKE_BUILD_TYPE Debug)
    message(STATUS "${Red}build_type Debug flag: ${CMAKE_CXX_FLAGS_DEBUG}${ColourReset}")
else(BUILD_DEBUG)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
    #add_definitions(-DNP_DEBUG)
    message(STATUS "${Red}build_type RelWithDebInfo flag: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}${ColourReset}")
endif(BUILD_DEBUG)

include_directories( ../xtra_rhelz.x/include )

aux_source_directory(. DIR_LIB_SRCS)
add_library (Clotho STATIC ${DIR_LIB_SRCS})


if(BUILD_DEBUG)
    message(STATUS "${Red}we will build examples and unit_tests ...${ColourReset}")
    
    find_program(MEMORYCHECK_COMMAND NAMES valgrind)
    if(MEMORYCHECK_COMMAND)
        # ctest -T memcheck
        set(MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full")
        include(CTest)
    endif()

    enable_testing()
    add_subdirectory( test )

endif()
