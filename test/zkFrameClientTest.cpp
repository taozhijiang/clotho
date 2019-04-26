#include <gmock/gmock.h>
#include <string>

#include <memory>
#include <iostream>

#include "zkFrame.h"

using namespace ::testing;
using namespace Clotho;

class FrameClientTest : public ::testing::Test {

protected:

    void SetUp() {
        bool ret =  client_.init("127.0.0.1:2181,127.0.0.1:2182", "aliyun");
        ASSERT_THAT(ret, Eq(true));

        std::map<std::string, std::string> properties = {
            { "ppa", "ppa_val" },
        };

        NodeType node("dept", "srv_inst", "0.0.0.0:1222", properties);
        ASSERT_THAT(client_.register_node(node, false), 0);

        node.node_ = "0.0.0.0:1223";
        ASSERT_THAT(client_.register_node(node, false), 0);

        node.node_ = "0.0.0.0:1224";
        ASSERT_THAT(client_.register_node(node, false), 0);

    }

    void TearDown() {
        // client_.revoke_all_nodes();
        // 需要这个sleep来让zk处理所有revoke的回调事件，否则
        // 会导致回调执行和client析构造成的死锁问题
        // ::sleep(1);
    }

public:
    zkFrame client_;
};

TEST_F(FrameClientTest, ClientPickNodeTest) {

    // watch service
    ASSERT_THAT(client_.subscribe_service("dept", "srv_inst", 0), Eq(0));

    NodeType node_g{};
    ASSERT_THAT(client_.pick_service_node("dept", "srv_inst", node_g), Eq(0));
}


