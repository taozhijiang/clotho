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
#include "zkRecipe.h"

// 如果获取网络环境异常，zkFrame的构造就抛出该异常
#include "ConstructException.h"

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

class zkFrame {

    FRIEND_TEST(zkFrameTest, ClientRegisterTest);

    friend class zkRecipe;

public:
    explicit zkFrame(const std::string& idc);
    ~zkFrame();

    // 禁止拷贝
    zkFrame(const zkFrame&) = delete;
    zkFrame& operator=(const zkFrame&) = delete;

    bool init(const std::string& hostline);

    // 注册服务提供节点，override表示是否覆盖现有的属性值
    int register_node(const NodeType& node, bool overwrite);

    // 解注册节点，服务禁用，为了让调用业务尽快感知，在服务下线的时候需要主动调用
    int revoke_node(const std::string& node_path);
    int revoke_all_nodes();

    // watch specific service
    int subscribe_service(const std::string& department, const std::string& service,
                          uint32_t strategy, bool with_nodes);

    // 特定的服务选择算法实现
    // 根据subscribe时候的策略进行选择
    int pick_service_node(const std::string& department, const std::string& service,
                          NodeType& node);
    // 手动自定义选择
    int pick_service_node(const std::string& department, const std::string& service,
                          uint32_t strategy, NodeType& node);

    // 注册制定路径的属性回调函数
    // 此处传入的路径只应该是服务节点，并且只有对应的服务被Watch了才有可能在属性变更的时候得到回调
    // 此处可能有点矛盾：即属性变更如果用作应用层的配置，那么应该是服务的发布节点
    //                   比较关注，但是sub应该是服务的消费节点比较关注的，所以如果
    //                   需要使用这个功能，那么服务的发布节点也需要sub服务才能实现对应功能
    int recipe_attach_node_property_cb(const std::string& dept, const std::string& service, uint16_t port,
                                       const NodePropertyCall& func) {
        std::string node = primary_node_addr_ + ":" + Clotho::to_string(port);
        return recipe_attach_node_property_cb(dept, service, node, func);
    }
    int recipe_attach_node_property_cb(const std::string& dept, const std::string& service, const std::string& node,
                                       const NodePropertyCall& func);

    int recipe_attach_serv_property_cb(const std::string& dept, const std::string& service, const ServPropertyCall& func);

    // sec <= 0, 不阻塞，立即返回结果
    // sec > 0, 阻塞的时间，以sec计数
    bool recipe_service_try_lock(const std::string& dept, const std::string& service, const std::string& lock_name, uint32_t sec);
    // 永久阻塞，直到成功
    bool recipe_service_lock(const std::string& dept, const std::string& service, const std::string& lock_name);
    // 主动解锁
    bool recipe_service_unlock(const std::string& dept, const std::string& service, const std::string& lock_name);
    // 是否是锁的持有者
    bool recipe_service_lock_owner(const std::string& dept, const std::string& service, const std::string& lock_name);


    // 提供外部可以周期性调用的刷新函数，ZooKeeper可能会有事件丢失，所以加上这个功能
    // 用户可以调用定时器接口自动进行服务的注册(刷新节点和配置数据)
    int periodicly_care();

    int set_priority(NodeType& node, uint16_t priority);
    int adj_priority(NodeType& node, int16_t step);
    int set_weight(NodeType& node, uint16_t weight);
    int adj_weight(NodeType& node, int16_t step);

    std::string primary_node_addr() const {
        return primary_node_addr_;
    }

private:
    //
    int substitute_node(const NodeType& node, std::vector<NodeType>& nodes);

    int internal_subscribe_node(NodeType& node);

    // rewatch的时候调用
    // overwrite 用于控制是否覆盖本地的weight, priority设置
    int internal_subscribe_service(const std::string& department, const std::string& service);
    int internal_subscribe_node(const char* node_path);

private:
    std::unique_ptr<zkClient> client_;
    std::unique_ptr<zkRecipe> recipe_;

    // 扩充网卡所在主机的地址信息，所有的地址信息会放到whole_nodes_addr，而只会选择一个
    // 网络信息放到primary_node_addr_中，
    // 对于选主、分布式锁等应用，我们只根据primary_node_addr_节点信息进行操作

    // 这些信息在构造完成后就不会改变，可以当作不变式使用

    const std::string idc_;
    std::string primary_node_addr_;
    std::vector<std::string> whole_nodes_addr_;


    std::mutex lock_;

    // 记录本地需要注册发布的服务信息
    // dept-srv-node 全路径作为键
    std::shared_ptr<MapNodeType>    pub_nodes_;

    // 记录本地需要订阅的服务信息
    // dept-srv 全路径作为键
    std::shared_ptr<MapServiceType> sub_services_;

    int handle_zk_event(int type, int state, const char* path);

    int internal_handle_zk_service_event(int type, const char* service_path);
    int internal_handle_zk_service_properties_event(int type, const char* service_property_path);
    int internal_handle_zk_node_event(int type, const char* node_path);
    int internal_handle_zk_node_properties_event(int type, const char* node_property_path);
};

} // Clotho

#endif // __CLOTHO_FRAME_H__

