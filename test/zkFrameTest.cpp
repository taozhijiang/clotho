#include <gmock/gmock.h>
#include <string>

#include <memory>
#include <iostream>

#include "zkFrame.h"

using namespace ::testing;
using namespace Clotho;

class FrameTest : public ::testing::Test {

protected:

    void SetUp() {
        bool ret =  client_.init("127.0.0.1:2181,127.0.0.1:2182", "aliyun");
        ASSERT_THAT(ret, Eq(true));
    }

    void TearDown() {
        client_.revoke_all_nodes();
    }

public:
    zkFrame client_;
};

int callback_serv(const std::string& expect,
                  const std::string& notify, const std::map<std::string, std::string>& property) {
    if (expect != notify)
        return -1;

    std::cout << " source " << notify << std::endl;
    std::cout << " propertiy: " << std::endl;
    for (auto iter = property.begin(); iter != property.end(); ++iter) {
        std::cout << "    " << iter->first << ", " << iter->second << std::endl;
    }
    std::cout << std::endl;

    return 0;
}

TEST_F(FrameTest, ClientRegisterTest) {

    std::map<std::string, std::string> properties = {
        { "ppa", "ppa_val" },
    };

    NodeType node("depart_a", "serv_a", "0.0.0.0:1222", properties);
    ASSERT_THAT(client_.register_node(node, false), 0);

    // watch service
    ASSERT_THAT(client_.subscribe_service("depart_a", "serv_a", 0), Eq(0));

    ASSERT_THAT(client_.register_property_cb("/depart_a/serv_a",
                                             std::bind(callback_serv, "/depart_a/serv_a",
                                                       std::placeholders::_1, std::placeholders::_2)),
                Eq(0));

    ::sleep(10000);
}


