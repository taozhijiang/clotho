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
        client_ = new zkFrame("aliyun");
        bool ret =  client_->init("127.0.0.1:2181,127.0.0.1:2182");
        ASSERT_THAT(ret, Eq(true));
    }

    void TearDown() {
        delete client_;
    }

public:
    zkFrame* client_;
};

int callback_node(const std::string& dept, const std::string& serv, const std::string& node, 
                  const std::map<std::string, std::string>& property) {

    std::cout << " source " << node << std::endl;
    std::cout << " propertiy: " << std::endl;
    for (auto iter = property.begin(); iter != property.end(); ++iter) {
        std::cout << "    " << iter->first << ", " << iter->second << std::endl;
    }
    std::cout << std::endl;

    return 0;
}

int callback_serv(const std::string& dept, const std::string& serv,
                  const std::map<std::string, std::string>& property) {
    
    std::cout << " source " << serv << std::endl;
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

    NodeType node("dept", "srv_inst", "0.0.0.0:1222", properties);
    ASSERT_THAT(client_->register_node(node, false), 0);

    // watch service
    ASSERT_THAT(client_->subscribe_service("dept", "srv_inst", 0, true), Eq(0));

    ASSERT_THAT(client_->recipe_attach_node_property_cb("dept", "srv_inst", 1222, callback_node), Eq(0));
    ASSERT_THAT(client_->recipe_attach_serv_property_cb("dept", "srv_inst", callback_serv), Eq(0));

    ASSERT_THAT(client_->recipe_service_lock("dept", "srv_inst", "master"), Eq(true));
    ASSERT_THAT(client_->recipe_service_lock_owner("dept", "srv_inst", "master"), Eq(true));


    ASSERT_THAT(client_->recipe_service_unlock("dept", "srv_inst", "master"), Eq(true));
    ASSERT_THAT(client_->recipe_service_lock_owner("dept", "srv_inst", "master"), Eq(false));

    ASSERT_THAT(client_->revoke_all_nodes(), Eq(0));
    ::sleep(1);
}


