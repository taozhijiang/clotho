/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __CLOTHO_NODE_H__
#define __CLOTHO_NODE_H__

#include <vector>
#include <string>
#include <map>

#include <sstream>


#define kWPMax            100
#define kWPMin            1
#define kWPDefault        50

namespace Clotho {

class NodeType;
class ServiceType;
typedef std::map<std::string, NodeType>    MapNodeType;
typedef std::map<std::string, ServiceType> MapServiceType;

typedef std::vector<std::pair<std::string, std::string>> VectorPair;


// NodeType的properties中，对于应用程序使用的配置建议以cfg_为前缀统一命名，而
// 目前框架使用的保留的属性键有：
// 1. active 临时节点，值为“1”，表明该节点是否存活；
// 2. idc    节点所在idc，如果节点选择算法包含kStrategyIdc，则会用到改值；
// 3. priority & weight 节点配置的优先级和权重，范围1-100，默认为50；
// 4. birth  临时节点，最新一次的发布日期时间


class NodeType {
public:

    NodeType(const std::string& department, const std::string& service, const std::string& node,
             const std::map<std::string, std::string>& properties = std::map<std::string, std::string>());

    // 供标准容器使用，需要支持默认构造
    NodeType() = default;
    ~NodeType() = default;

    bool available() {
        return active_ && enabled_;
    }

    // TODO operator <<
    std::string str() const;
    bool prepare_path(VectorPair& paths);

    static bool node_parse(const char* fp, std::string& d, std::string& s, std::string& n);
    static bool node_property_parse(const char* fp,
                                    std::string& d, std::string& s, std::string& n, std::string& p);

public:
    std::string department_;
    std::string service_;
    std::string node_;  // ip:port

    // 发现服务的时候解析并填写进来
    std::string host_;
    uint16_t    port_;

    // watch时候填充的状态，便于快速筛选
    bool        active_;    // 远程active节点的值
    bool        enabled_;   // 本地是否禁用

    // 默认初始化是远程节点的值，除非
    // 1. 本地可以手动修改该值，表明是以本地的视角考量的服务
    // 2. 如果远程配置更新了这些值，则会默认覆盖掉本地修改值
    std::string idc_;
    uint16_t    priority_;  // 1~100，默认50, 越小优先级越高
    uint16_t    weight_;    // 1~100，默认50

    std::map<std::string, std::string> properties_;

    friend std::ostream& operator<<(std::ostream& os, const NodeType& node);
};



// ServiceType的properties中，我们主要提供的是服务治理相关的属性，不支持应用程序的配置参数
// 目前框架使用的保留的属性键有：
// 1. lock_xxx-xx   临时节点，服务级别的分布式互斥锁的实现，其值为节点名

class ServiceType {

public:
    ServiceType(const std::string& department, const std::string& service,
                const std::map<std::string, std::string>& properties = std::map<std::string, std::string>());

    // 供标准容器使用，需要支持默认构造
    ServiceType() = default;
    ~ServiceType() = default;

    bool available() {
        return enabled_;
    }

    std::string str() const;

    static bool service_parse(const char* fp, std::string& d, std::string& s);
    static bool service_property_parse(const char* fp,
                                       std::string& d, std::string& s, std::string& p);

public:
    std::string department_;
    std::string service_;

    bool        enabled_;    // 本地是否禁用

    uint32_t    pick_strategy_;
    bool        with_nodes_; // 表示是否需要侦听nodes_节点信息，如果false则只关注properties

    std::map<std::string, NodeType> nodes_;

    std::map<std::string, std::string> properties_;

    friend std::ostream& operator<<(std::ostream& os, const ServiceType& srv);
};


} // Clotho

#endif // __CLOTHO_FRAME_H__

