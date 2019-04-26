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
};


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

    bool        enabled_;   // 本地是否禁用

    uint32_t pick_strategy_;
    std::map<std::string, NodeType> nodes_;

    std::map<std::string, std::string> properties_;
};


} // Clotho

#endif // __CLOTHO_FRAME_H__

