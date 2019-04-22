#ifndef __CLOTHO_FRAME_H__
#define __CLOTHO_FRAME_H__

#include <mutex>
#include <memory>
#include <string>
#include <map>

#include <sstream>
#include <iostream>

#include "zkPath.h"
#include "zkNode.h"
#include "zkClient.h"

#include <gtest/gtest_prod.h>

// 客户端使用的上层API

namespace Clotho {

class zkFrame {

    FRIEND_TEST(zkFrameTest, ClientRegisterTest);

public:
    zkFrame() :
        client_(),
        lock_(),
        pub_nodes_(),
        sub_services_() {
    }

    ~zkFrame() { }

    // 禁止拷贝
    zkFrame(const zkFrame&) = delete;
    zkFrame& operator=(const zkFrame&) = delete;

    bool init(const std::string& hostline, const std::string& idc);

    // 注册服务提供节点
    int register_node(const NodeType& node, bool overwrite);

    // 解注册节点，服务禁用，为了让调用业务尽快感知，在服务下线的时候需要主动调用
    int revoke_node(const std::string& node_path);
    int revoke_all_nodes();

    // watch specific service
    int subscribe_service(const std::string& department, const std::string& service,
                          uint32_t strategy);

    // 特定的服务选择算法实现
    // 根据subscribe时候的策略进行选择
    int pick_service_node(const std::string& department, const std::string& service,
                          NodeType& node);
    // 手动自定义选择
    int pick_service_node(const std::string& department, const std::string& service,
                          uint32_t strategy, NodeType& node);

    int set_priority(NodeType& node, uint16_t priority) {
        uint16_t original = node.priority_;

        node.priority_ = priority;
        return original;
    }

    int adj_priority(NodeType& node, int16_t step) {
        uint16_t original = node.priority_;

        int16_t total = node.priority_ + step;
        total = total < kWPMin ? kWPMin : total;
        total = total > kWPMax ? kWPMax : total;
        node.priority_ = static_cast<uint16_t>(total);
        return original;
    }

    int set_weight(NodeType& node, uint16_t weight) {
        uint16_t original = node.weight_;

        node.weight_ = weight;
        return original;
    }

    int adj_weight(NodeType& node, int16_t step) {
        uint16_t original = node.weight_;

        int16_t total = node.weight_ + step;
        total = total < kWPMin ? kWPMin : total;
        total = total > kWPMax ? kWPMax : total;
        node.weight_ = static_cast<uint16_t>(total);
        return original;
    }

    void set_idc(NodeType& node, const std::string& idc) {
        if (!idc.empty())
            node.idc_ = idc;
    }

private:

    // 根据0.0.0.0扩充节点
    bool extend_substant_node(const NodeType& node, std::vector<NodeType>& nodes);

    int subscribe_node(NodeType& node);

    // rewatch的时候调用
    // overwrite 用于控制是否覆盖本地的weight, priority设置
    int subscribe_service(const std::string& department, const std::string& service);
    int subscribe_node(const char* node_path);

private:
    std::string idc_;
    std::unique_ptr<zkClient> client_;

    std::mutex lock_;

    // 记录本地需要注册发布的服务信息
    std::shared_ptr<MapNodeType>    pub_nodes_;

    // 记录本地需要订阅的服务信息
    std::shared_ptr<MapServiceType> sub_services_;

    int handle_zk_event(int type, int state, const char* path);

    int do_handle_zk_service_event(int type, const char* service_path);
    int do_handle_zk_service_properties_event(int type, const char* service_property_path);
    int do_handle_zk_node_event(int type, const char* node_path);
    int do_handle_zk_node_properties_event(int type, const char* node_property_path);
};

} // Clotho

#endif // __CLOTHO_FRAME_H__

