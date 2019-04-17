cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x " )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual -Wpointer-arith -Wwrite-strings -Werror=format -march=native " )
include("${PROJECT_SOURCE_DIR}/help.cmake")
define_colors()


include_directories(
        ${PROJECT_SOURCE_DIR}/
        ${PROJECT_SOURCE_DIR}/../xtra_rhelz.x/include/
        ${PROJECT_SOURCE_DIR}/../xtra_rhelz.x/include/google/
)

link_directories(
        ${PROJECT_SOURCE_DIR}/../xtra_rhelz.x/libs/
        ${PROJECT_SOURCE_DIR}/../xtra_rhelz.x/libs/zookeeper/
        ${PROJECT_SOURCE_DIR}/../xtra_rhelz.x/libs/boost/
        ${PROJECT_SOURCE_DIR}/../xtra_rhelz.x/libs/google/
        ${PROJECT_SOURCE_DIR}/lib
)

set (TEST_HOST_LIB Clotho)

set (EXTRA_LIBS ${EXTRA_LIBS} pthread zookeeper_mt)
set (EXTRA_LIBS ${EXTRA_LIBS} boost_system boost_thread)
set (EXTRA_LIBS ${EXTRA_LIBS} gtest gmock gtest_main)


add_individual_test(zkPath)
add_individual_test(zkClient)
add_individual_test(zkFrame)
