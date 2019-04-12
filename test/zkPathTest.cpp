#include <gmock/gmock.h>
#include <string>

#include <iostream>

#include "zkPath.h"

using namespace ::testing;
using namespace Clotho;

TEST(zkPathTest, PathTest) {

    ASSERT_THAT(zkPath::department_from_path("/prj/srv1/test"), Eq("prj"));
    ASSERT_THAT(zkPath::service_from_path("/prj/srv1/test"), Eq("srv1"));

    std::vector<std::string> ips = zkPath::get_local_ips();
    ASSERT_THAT(ips.size(), Gt(0));
    std::cout << "ip: " << ips[0] << std::endl;
}

TEST(zkPathTest, SplitTest) {

    std::vector<std::string> vec {};
    zkPath::split("////prj/test ?te2", "/?", vec);
    
    ASSERT_THAT(vec.size(), Eq(3));
    ASSERT_THAT(vec[0], Eq("prj"));
    ASSERT_THAT(vec[1], Eq("test "));
    ASSERT_THAT(vec[2], Eq("te2"));
    
    std::string bs = zkPath::base_name("/bas/ths1/ts");
    ASSERT_THAT(bs, Eq("ts"));
}


TEST(zkPathTest, PathPureAndValidateTest) {

    ASSERT_THAT(zkPath::normalize_path("//prj/test"), Eq("/prj/test"));
    ASSERT_THAT(zkPath::normalize_path("  //prj/test"), Eq("/prj/test"));
    ASSERT_THAT(zkPath::normalize_path("//prj/test// "), Eq("/prj/test"));
    ASSERT_THAT(zkPath::normalize_path("//prj/ test"), Eq("/prj/ test"));
    
    ASSERT_THAT(zkPath::validate_node("2015.3.3.1:1003"), Eq(false));
    ASSERT_THAT(zkPath::validate_node("20.3.3.1:1003"), Eq(true));
    
    std::string ip; uint16_t port;
    ASSERT_THAT(zkPath::validate_node("20.3.3.1:1003", ip, port), Eq(true));
    ASSERT_THAT(ip, Eq("20.3.3.1"));
    ASSERT_THAT(port, Eq(1003));
}

