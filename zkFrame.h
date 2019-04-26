#ifndef __CLOTHO_FRAME_H__
#define __CLOTHO_FRAME_H__

#include <mutex>
#include <memory>
#include <string>
#include <map>

#include <functional>

#include <sstream>
#include <iostream>

#include "zkPath.h"
#include "zkNode.h"
#include "zkClient.h"

#include <gtest/gtest_prod.h>

// 如果选择该项，就会进行IDC筛选，
// 如果IDC筛选后节点为空了，就会忽略该参数
#define kStrategyIdc        (0x1u<<0)

// 下面三种选择算法是互斥的，按照该优先级处理
#define kStrategyRandom     (0x1u<<2)
#define kStrategyRoundRobin (0x1u<<3)
#define kStrategyWP         (0x1u<<4)

#define kStrategyDefault    (kStrategyIdc | kStrategyWP)



// 客户端使用的上层API

namespace Clotho {


typedef std::function<int(const std::string& notify,\
                              const std::map<std::string, std::string>& properties)> PropertyCall;

class zkFrame {

    FRIEND_TEST(zkFrameTest, ClientRegisterTest);

public:
    zkFrame() :
        client_(),
        lock_(),
        pub_nodes_(),
        sub_services_() {
    }

    ~zkFrame() {
        std::lock_guard<std::mutex> lock(lock_);
        client_.reset();
    }

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

    // 注册制定路径的属性回调函数
    // 此处传入的路径只应该是节点路径或者服务路径，并且只有对应的服务被Watch了才
    // 可能在属性变更的时候得到回调
    // 此处可能有点矛盾：即属性变更如果用作应用层的配置，那么应该是服务的发布节点
    //                   比较关注，但是sub应该是服务的消费节点比较关注的，所以如果
    //                   需要使用这个功能，那么服务的发布节点也需要sub服务才能实现对应功能
    int register_property_cb(const std::string& path, const PropertyCall& func) {

        std::lock_guard<std::mutex> lock(lock_);
        std::string n_path = zkPath::normalize_path(path);
        PathType pt = zkPath::guess_path_type(n_path);
        if (func && (pt == PathType::kNode || pt == PathType::kService)) {
            property_callmap_[n_path] = func;
            return 0;
        }

        return -1;
    }

    // 提供外部可以周期性调用的刷新函数，ZooKeeper可能会有事件丢失，所以加上这个功能
    // 用户可以调用定时器接口自动进行服务的注册(刷新节点和配置数据)
    int periodic_takecare() {

        std::vector<std::string> services{};

        {
            std::lock_guard<std::mutex> lock(lock_);
            for (auto iter = sub_services_->begin(); iter != sub_services_->end(); ++iter)
                services.push_back(iter->first);
        }

        for (size_t i = 0; i < services.size(); ++i) {
            std::string depart;
            std::string service;
            if (ServiceType::service_parse(services[i].c_str(), depart, service))
                subscribe_service(depart, service);
        }

        return 0;
    }

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
    bool expand_substant_node(const NodeType& node, std::vector<NodeType>& nodes);

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
    // dept-srv-node 全路径作为键
    std::shared_ptr<MapNodeType>    pub_nodes_;

    // 记录本地需要订阅的服务信息
    // dept-srv 全路径作为键
    std::shared_ptr<MapServiceType> sub_services_;

    // 属性变更的回调列表
    std::map<std::string, PropertyCall> property_callmap_;

    int handle_zk_event(int type, int state, const char* path);

    int do_handle_zk_service_event(int type, const char* service_path);
    int do_handle_zk_service_properties_event(int type, const char* service_property_path);
    int do_handle_zk_node_event(int type, const char* node_path);
    int do_handle_zk_node_properties_event(int type, const char* node_property_path);
};

} // Clotho

#endif // __CLOTHO_FRAME_H__

