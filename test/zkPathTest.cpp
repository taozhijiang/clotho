#include <gmock/gmock.h>
#include <string>

#include <iostream>

#include "zkPath.h"

using namespace ::testing;

namespace Clotho {

TEST(zkPathTest, PathTest) {

    std::vector<std::string> ips = zkPath::get_local_ips();
    ASSERT_THAT(ips.size(), Gt(0));
    std::cout << "ip: " << ips[0] << std::endl;
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

TEST(zkPathTest, PathTypeTest) {

    ASSERT_THAT(zkPath::guess_path_type("//prjjl"), Eq(PathType::kDepartment));

    ASSERT_THAT(zkPath::guess_path_type("//prjjl/sss"), Eq(PathType::kService));
    ASSERT_THAT(zkPath::guess_path_type("//prjjl/sss/master_node"), Eq(PathType::kServiceMaster));
    ASSERT_THAT(zkPath::guess_path_type("//prjjl/sss/Master"), Eq(PathType::kServiceProperty));

    ASSERT_THAT(zkPath::guess_path_type("//prjjl/sss/172.20.11.11:100"), Eq(PathType::kNode));
    ASSERT_THAT(zkPath::guess_path_type("//prjjl/sss/ssss/ssss"), Eq(PathType::kUndetected));
    ASSERT_THAT(zkPath::guess_path_type("//prjjl/sss/172.20.11.11:200/ssss"), Eq(PathType::kNodeProperty));

}

}  // end Clotho
