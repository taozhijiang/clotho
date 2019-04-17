#ifndef __CLOTHO_FRAME_H__
#define __CLOTHO_FRAME_H__

#include <mutex>
#include <memory>
#include <string>
#include <map>

#include "zkClient.h"

// 客户端使用的上层API

namespace Clotho {

#define kStrategyIdc      (0x1u<<0)
#define kStrategyPriority (0x1u<<1)
#define kStrategyWeight   (0x1u<<2)
#define kStrategyDefault  (kStrategyIdc|kStrategyPriority|kStrategyWeight)

#define kStrategyRandom   (0x1u<<10)
#define kStrategyRound    (0x1u<<11)

class NodeType;
class ServiceType;
typedef std::map<std::string, NodeType>    MapNodeType;
typedef std::map<std::string, ServiceType> MapServiceType;

class NodeType {
public:

    NodeType(const std::string& department, const std::string& service, const std::string& node,
             const std::map<std::string, std::string>& properties) :
        department_(department), service_(service), node_(node),
        host_(), port_(0),
        active_(false), enabled_(true),
        idc_(), priority_(), weight_(),
        properties_(properties) {
    }

    bool available() {
        return active_ && enabled_;
    }

    std::string str() const {
        return "";
    }

public:
    const std::string department_;
    const std::string service_;
    const std::string node_;  // ip:port

    // 发现服务的时候解析并填写进来
    std::string host_;
    uint16_t    port_;

    // watch时候填充的状态，便于快速筛选
    bool active_;    // 远程active节点的值
    bool enabled_;   // 本地是否禁用

    // 默认初始化是远程节点的值，除非
    // 1. 本地可以手动修改该值，表明是以本地的视角考量的服务
    // 2. 如果远程配置更新了这些值，则会默认覆盖掉本地修改值
    std::string idc_;
    uint8_t     priority_;  // 0~255，默认128，越小优先级越高
    uint8_t     weight_;    // 0~255，默认128

    std::map<std::string, std::string> properties_;
};


class ServiceType {
public:
    ServiceType(const std::string& department, const std::string& service,
                const std::map<std::string, std::string>& properties) :
        department_(department), service_(service),
        active_(false), enabled_(true),
        pick_strategy_(kStrategyDefault),
        nodes_(),
        properties_(properties) {
    }

    bool available() {
        return active_ && enabled_;
    }

    std::string str() const {
        return "";
    }

public:
    const std::string department_;
    const std::string service_;

    bool active_;    // 远程active节点的值
    bool enabled_;   // 本地是否禁用

    uint32_t pick_strategy_;
    std::map<std::string, NodeType>    nodes_;

    std::map<std::string, std::string> properties_;
};


class zkFrame {

public:
    zkFrame() :
        client_(),
        lock_(),
        pub_nodes_(),
        sub_services_() {
    }

    ~zkFrame() { }

    zkFrame(const zkFrame&) = delete;
    zkFrame& operator=(const zkFrame&) = delete;

    bool init(const std::string& hostline, const std::string& idc);

    // 注册服务提供节点
    int register_node(const NodeType& node);

    // 解注册节点，服务禁用
    int revoke_node(const std::string& node_path);

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

    int set_priority(NodeType& node, uint8_t priority);
    int adj_priority(NodeType& node, int8_t  step);

    int set_weight(NodeType& node, uint8_t weight);
    int adj_weight(NodeType& node, int8_t  step);

    std::string set_idc(NodeType& node, const std::string& idc);

    // 导出注册节点信息和订阅服务信息
    std::string dump_pub_nodes();
    std::string dump_sub_services();

private:
    std::unique_ptr<zkClient> client_;

    std::mutex lock_;

    // 记录本地需要注册发布的服务信息
    std::shared_ptr<MapNodeType>    pub_nodes_;

    // 记录本地需要订阅的服务信息
    std::shared_ptr<MapServiceType> sub_services_;

    int handle_zk_event(int type, int state, const char* path);
};

} // Clotho

#endif // __CLOTHO_FRAME_H__

