cmake_minimum_required (VERSION 2.8.11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x " )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual -Wpointer-arith -Wwrite-strings -Werror=format -march=native " )


include_directories(
        ../
        ../../xtra_rhelz.x/include/
        ../../xtra_rhelz.x/include/google/
)

link_directories(
        ../../xtra_rhelz.x/libs/
        ../../xtra_rhelz.x/libs/zookeeper/
        ../../xtra_rhelz.x/libs/boost/
        ../../xtra_rhelz.x/libs/google/
        ../lib
)


set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

aux_source_directory( ./ UNIT_TEST_SRCS )
add_executable( test ${UNIT_TEST_SRCS} )

set (EXTRA_LIBS tzkeeper)
set (EXTRA_LIBS ${EXTRA_LIBS} zookeeper_mt)
set (EXTRA_LIBS ${EXTRA_LIBS} boost_system )
set (EXTRA_LIBS ${EXTRA_LIBS} gtest gmock)

target_link_libraries( test -lrt -rdynamic -ldl
    ${EXTRA_LIBS}
)

