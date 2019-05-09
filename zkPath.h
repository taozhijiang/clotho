/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __CLOTHO_PATH_H__
#define __CLOTHO_PATH_H__

#include <string>
#include <cstring>
#include <vector>
#include <limits>
#include <sstream>

#include <gtest/gtest_prod.h>


// replace with Log.h latter
#include <cstdio>
#ifndef log_info
#define log_info(fmt, ...) ::printf("DEBUG [%s:%d(%s)]" fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#endif
#ifndef log_warning
#define log_warning(fmt, ...)  ::printf("TRACE [%s:%d(%s)]" fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#endif
#ifndef log_err
#define log_err(fmt, ...)   ::printf("ERROR [%s:%d(%s)]" fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#endif

namespace Clotho {

// 约定的服务组织模式： /department/service/node

enum class PathType : uint8_t {
    kDepartment     = 1,

    kService        = 2,
    kServiceProperty = 3,

    kNode           = 10,
    kNodeProperty   = 11,

    kUndetected     = 100,
};

class zkPath {

    FRIEND_TEST(zkPathTest, ClientRegisterTest);

public:

    // 获取本主机的IP地址信息，出去回环、非法地址
    static std::vector<std::string> get_local_ips();
    static std::string get_local_ip();


    static enum PathType guess_path_type(const std::string& path);

    // 空白的元素不会添加到vec结果中去
    static void split(const std::string& str,
                      const std::string& needle, std::vector<std::string>& vec);

    static std::string make_path(const std::string& d, const std::string& s) {
        return "/" + d + "/" + s;
    }

    static std::string make_path(const std::string& d, const std::string& s, const std::string& n) {
        return "/" + d + "/" + s + "/" + n;
    }

    static std::string extend_property(const std::string& fp, const std::string& p) {
        return fp + "/" + p;
    }

    // 优化路径名字，包括：删除空白字符，删除中间连续的以及末尾的 '/'
    static std::string normalize_path(const std::string& str);

    // ip:port node_name strict
    static bool validate_node(const std::string& node_name);

    // ip:port node_name strict
    static bool validate_node(const std::string& node_name, std::string& ip, uint16_t& port);

};

template<typename T>
std::string to_string(const T& t) {
    std::stringstream ss;
    ss << t;
    return ss.str();
}

} // Clotho

#endif // __CLOTHO_PATH_H__

