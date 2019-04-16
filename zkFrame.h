#ifndef __CLOTHO_FRAME_H__
#define __CLOTHO_FRAME_H__


#include <memory>
#include <string>
#include <map>

#include "zkClient.h"

// 客户端使用的上层API

namespace Clotho {

enum PickServiceStrategy: uint8_t {
    kByWeight = 1,
    kByPriority = 2,
    kByIdc = 3,
    kByMaster = 4,
};

struct NodeType {

    std::string department_;
    std::string service_;
    std::string node_;

    std::string host_;
    uint16_t    port_;

    std::map<std::string, std::string> properties_;
};


struct ServiceType {

    std::string department_;
    std::string service_;

    std::map<std::string, std::string> properties_;
};

class zkFrame {

public:
    zkFrame() {}
    ~zkFrame() {}

    bool init(const std::string& hostline, const std::string& idc);

    // upload our service instance
    int publish_node(const NodeType& node);
    int revoke_node(const std::string& node_path);

    // watch specific service
    int subscribe_service(const std::string& department, const std::string& service);

    // 特定的服务选择算法实现
    int pick_service_node(NodeType& node);

private:
    std::unique_ptr<zkClient> client_;

    // 记录本地需要注册发布的服务信息
//    std::map<std::string> published_nodes_;

    // 记录本地需要订阅的服务信息
//    std::map<std::string> subscribe_services_;

    int handle_zk_event(int type, int state, const char *path);

    // some help func
    std::string make_full_path(const std::string& department, const std::string& service, const std::string& node);
    int prase_from_path(const char *path, std::string& department, std::string& service, std::string& node);
};

} // Clotho

#endif // __CLOTHO_FRAME_H__

