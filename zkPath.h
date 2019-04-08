#ifndef __ZK_PATH_H__
#define __ZK_PATH_H__

#include <string>
#include <vector>

#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>

namespace tzkeeper {

// 约定的服务组织模式： /department/service/node
class zkPath {

public:

    static std::string department_from_path(const std::string& path) {
        if(path.empty() || path[0] != '/')
            return "";

        std::vector<std::string> vec {};
        split(path, "/", vec);
        if(vec.size() >= 1)
            return vec[0];

        return "";
    }

    static std::string service_from_path(const std::string& path) {
        if(path.empty() || path[0] != '/')
            return "";

        std::vector<std::string> vec {};
        split(path, "/", vec);
        if(vec.size() >= 2)
            return vec[1];

        return "";
    }

    // 获取本主机的IP地址信息，出去回环、非法地址
    static std::vector<std::string> get_local_ips() {

        struct ifaddrs* ifaddr = NULL;
        int family = 0;
        int ec = 0;
        char host[NI_MAXHOST] {};
        std::vector<std::string> ips {};

        if (getifaddrs(&ifaddr) == -1) {
            fprintf(stderr, "getifaddrs failed. errno: %d:%s", errno, strerror(errno));
            return ips;
        }

        for(struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if(ifa->ifa_addr == NULL)
                continue;

            if(ifa->ifa_name == NULL || ::strncasecmp(ifa->ifa_name, "lo", strlen("lo")) == 0)
                continue;

            family = ifa->ifa_addr->sa_family;
            if(family != AF_INET)
                continue;

            ec = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                    host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if(ec != 0) {
                fprintf(stderr, "getnameinfo() failed: %s", gai_strerror(ec));
                continue;
            }

            if(strncmp(host, "127", strlen("127")) == 0 || strncmp(host, "169.254", strlen("169.254") == 0)) {
                // ignore localhost, invalid addr
                continue;
            }

            ips.push_back(host);
        }

        freeifaddrs(ifaddr);
        return ips;
    }


    // 优化路径名字，包括：删除空白字符，删除中间连续的以及末尾的 '/'
    static std::string normalize_path(const std::string& str) {
        
        std::string copy_str = str;
        size_t index = 0;

        // trim left whitespace
        for (index = 0; index < copy_str.size() && isspace(copy_str[index]); ++index)
            /* do nothing*/;
        copy_str.erase(0, index);

        // trim right whitespace
        for (index = copy_str.size(); index > 0 && isspace(copy_str[index - 1]); --index)
            /* do nothing*/;
        copy_str.erase(index);

        std::string result {};
        for(size_t i = 0; i < copy_str.size(); ++i) {
            if(copy_str[i] == '/' && !result.empty() && result.at(result.size()-1) == '/') {
                continue;
            }
            result.push_back(copy_str[i]);
        }

        if(!result.empty() && result.at(result.size()-1) == '/')
            result.erase(result.size()-1);

        return result;
    }

    static void split(const std::string& str, const std::string& needle, std::vector<std::string>& vec) {

        std::string::size_type pos = 0;
        std::string::size_type oldPos = 0;

        while (true) {
            pos = str.find_first_of(needle, oldPos);

            if (std::string::npos == pos) {
                auto item = str.substr(oldPos);
                if(!item.empty())
                    vec.push_back(item);
                break;
            }

            auto item = str.substr(oldPos, pos - oldPos);
            if(!item.empty())
                vec.push_back(item);

            oldPos = pos + 1;
        }
    }

    // ip:port node_name strict
    static bool validate_node(const std::string& node_name) {

        std::vector<std::string> vec{};
        split(node_name, ":.", vec);

        if(vec.size() != 5)
            return false;

        for(size_t i=0; i<4; ++i) {
            int num = ::atoi(vec[i].c_str());
            if(num < 0 || num > std::numeric_limits<uint8_t>::max())
                return false;
        }

        int port = ::atoi(vec[4].c_str());
        if(port <= 0 || port >= std::numeric_limits<uint16_t>::max())
            return false;

        return true;
    }

    // ip:port node_name strict
    static bool validate_node(const std::string& node_name, std::string& ip, uint16_t& port) {

        std::vector<std::string> vec{};
        split(node_name, ":.", vec);

        if(vec.size() != 5)
            return false;

        for(size_t i=0; i<4; ++i) {
            int num = ::atoi(vec[i].c_str());
            if(num < 0 || num > std::numeric_limits<uint8_t>::max())
                return false;
        }

        port = ::atoi(vec[4].c_str());
        if(port <= 0 || port >= std::numeric_limits<uint16_t>::max())
            return false;

        char ip_str[32] {};
        snprintf(ip_str, sizeof(ip_str), "%s.%s.%s.%s",
                                          vec[0].c_str(), vec[1].c_str(), vec[2].c_str(), vec[3].c_str());
        ip = ip_str;
        return true;
    }


    // 获取路径的目录、名字等信息，为了准确，推荐使用
    // normalize_path 对路径信息进行预处理
    static std::string base_name(const std::string& path) {
        size_t pos = path.rfind("/");
        if(pos == std::string::npos) {
            return "";
        }
        return path.substr(pos + 1);
    }

    static std::string dir_name(const std::string& path) {
        size_t pos = path.rfind("/");
        if(pos == 0) {
            return "/";
        }
        return path.substr(0, pos);
    }
};

} // end namespace tzkeeper

#endif // __ZK_PATH_H__

