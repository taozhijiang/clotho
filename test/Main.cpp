#include <iostream>
#include "gtest/gtest.h"

#include <xtra_rhel.h>

int main(int argc, char** argv) {

    testing::InitGoogleTest(&argc, argv);

    // do initialize

    return RUN_ALL_TESTS();
}



namespace boost {
void assertion_failed(char const* expr, char const* function, char const* file, long line) {
    fprintf(stderr, "BAD!!! expr `%s` assert failed at %s(%ld): %s", expr, file, line, function);
}
} // end boost
