#ifndef __ZK_FRAME_H__
#define __ZK_FRAME_H__


#include <memory>
#include <string>

#include "zkClient.h"

// 客户端使用的上层API

namespace tzkeeper {

class zkFrame {

public:
    zkFrame() {}
    ~zkFrame() {}

    bool init(const std::string& hostline, const std::string& idc);

    int set_priority(int priority = 1);
    int adjust_priority(int priority_step = 1);

    int set_weight(int weight = 1);
    int adjust_weight(int weight_step = 1);

    int activate(const std::string& node_path);
    int deactivate(const std::string& node_path);


    int publish_service();
    int consume_service();

private:
    std::unique_ptr<zkClient> client_;
};

} // end namespace tzkeeper

#endif // __ZK_FRAME_H__

