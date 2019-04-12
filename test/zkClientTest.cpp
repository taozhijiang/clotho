#include <gmock/gmock.h>
#include <string>

#include <memory>
#include <iostream>

#include "zkClient.h"

using namespace ::testing;
using namespace Clotho;

TEST(zkClientTest, PathTest) {

    auto client = std::make_shared<zkClient>("127.0.0.1:2181,127.0.0.1:2182,127.0.0.1:2183");
    bool client_ok = client;
    ASSERT_THAT(client_ok, Eq(true));

    ASSERT_THAT(client->zk_init(), Eq(true));
}
