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

class zkFrame {

public:
    zkFrame() {}
    ~zkFrame() {}

    bool init(const std::string& hostline, const std::string& idc);

    int set_priority(int val);
    int adj_priority(int priority_step);  // + - 增加减少

    int set_weight(int val);
    int adj_weight(int weight_step);

    // upload our service instance
    int pub_service(const std::string& department, const std::string& service);

    // watch specific service
    int sub_service(const std::string& department, const std::string& service);
    int pick_service_node();

private:
    std::unique_ptr<zkClient> client_;

    // 记录本地需要注册的服务信息
//    std::map<std::string> publish_services_;

    // 记录本地需要订阅的服务信息
//    std::map<std::string> subscribe_services_;

    int handle_zk_event(int type, int state, const char *path);

    // some help func
    std::string make_full_path(const std::string& department, const std::string& service, const std::string& node);
    int prase_from_path(const char *path, std::string& department, std::string& service, std::string& node);
};

} // Clotho

#endif // __CLOTHO_FRAME_H__

