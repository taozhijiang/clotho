#include <cassert>
#include <zookeeper/zookeeper.h>

#include "zkFrame.h"

namespace Clotho {

bool zkFrame::init(const std::string& hostline, const std::string& idc) {

    if (hostline.empty() || idc.empty()) {
        return false;
    }

    idc_ = idc;

    auto func = std::bind(&zkFrame::handle_zk_event, this,
                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    client_.reset(new zkClient(hostline, func, idc));
    if (!client_ || !client_->zk_init()) {
        log_err("create and init zkClient failed.");
        client_.reset();
        return false;
    }

    pub_nodes_ = std::make_shared<MapNodeType>();
    sub_services_ = std::make_shared<MapServiceType>();

    if (!pub_nodes_ || !sub_services_) {
        return false;
    }

    return true;
}


int zkFrame::register_node(const NodeType& node, bool overwrite) {

    std::vector<NodeType> nodes;
    if (!extend_substant_node(node, nodes)) {
        log_err("extend nodes failed.");
        return -1;
    }

    for (size_t i = 0; i < nodes.size(); ++i) {

        std::cout << nodes[i].str() << std::endl;

        VectorPair paths {};
        if (!nodes[i].prepare_path(paths)) {
            log_err("prepare path for %s failed.", nodes[i].node_.c_str());
            continue;
        }

        for (auto iter = paths.begin(); iter != paths.end(); ++iter) {
            PathType tp = zkPath::guess_path_type(iter->first);
            if (tp == PathType::kDepartment || tp == PathType::kService || tp == PathType::kNode) {
                client_->zk_create_if_nonexists(iter->first.c_str(), iter->second, &ZOO_OPEN_ACL_UNSAFE, 0);
            }
            else if (tp == PathType::kServiceProperty || tp == PathType::kNodeProperty) {
                if (overwrite)
                    client_->zk_create_or_update(iter->first.c_str(), iter->second, &ZOO_OPEN_ACL_UNSAFE, 0);
                else
                    client_->zk_create_if_nonexists(iter->first.c_str(), iter->second, &ZOO_OPEN_ACL_UNSAFE, 0);
            }
            else {
                log_err("Unknown PathType for %s", iter->first.c_str());
            }
        }

        // add additional active path
        std::string active_path =
                zkPath::extend_property(zkPath::make_path(nodes[i].department_, nodes[i].service_, nodes[i].node_),
                                        "active");

        client_->zk_create(active_path.c_str(), "1", &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL);

        {
            std::string full = zkPath::make_path(nodes[i].department_, nodes[i].service_, nodes[i].node_);
            full = zkPath::normalize_path(full);

            std::lock_guard<std::mutex> lock(lock_);
            (*pub_nodes_)[full] = nodes[i];

            log_debug("add %s into pub_nodes_, info: %s", full.c_str(), nodes[i].str().c_str());
        }
    }

    return 0;
}


int zkFrame::revoke_node(const std::string& node_path) {

    PathType tp = zkPath::guess_path_type(node_path);
    if (tp != PathType::kNode) {
        log_err("invalid node path: %s", node_path.c_str());
        return -1;
    }

    std::shared_ptr<MapNodeType> reg_nodes;
    {
        std::lock_guard<std::mutex> lock(lock_);
        pub_nodes_->erase(node_path);
    }

    std::string active_path = zkPath::extend_property(node_path, "active");
    client_->zk_delete(active_path.c_str());
    client_->zk_set(node_path.c_str(), "0");

    return 0;
}

int zkFrame::revoke_all_nodes() {

    std::shared_ptr<MapNodeType> reg_nodes;
    {
        std::lock_guard<std::mutex> lock(lock_);
        reg_nodes = pub_nodes_;
    }

    for (auto iter = reg_nodes->begin(); iter != reg_nodes->end(); ++iter) {
        revoke_node(iter->first);
    }

    return 0;
}


// subscribe_service should also subscribe its node instance
// subscribe_service will update corresponding param and value with zk_get
int zkFrame::subscribe_service(const std::string& department, const std::string& service,
                               uint32_t strategy) {

    std::string service_path = zkPath::make_path(department, service);
    if (zkPath::guess_path_type(service_path) != PathType::kService) {
        log_err("invalid service path: %s", service_path.c_str());
        return -1;
    }

    ServiceType srv(department, service);

    std::string value;
    int code = client_->zk_get(service_path.c_str(), value, 1, NULL);
    if (code != 0) {
        log_err("get service %s failed.", service_path.c_str());
        return -1;
    }

    srv.enabled_ = ( value == "1" );
    srv.pick_strategy_ = strategy ? strategy : kStrategyDefault;

    // 处理子节点
    std::vector<std::string> sub_path {};
    code = client_->zk_get_children(service_path.c_str(), 1, sub_path);
    if (code != 0) {
        log_err("get service children node failed %d", code);
        return -1;
    }

    for (size_t i=0; i<sub_path.size(); ++i) {

        std::string sub_node = service_path + "/" + sub_path[i];
        PathType tp = zkPath::guess_path_type(sub_node);
        if (tp == PathType::kServiceProperty)
        {
            if( client_->zk_get(sub_node.c_str(), value, 1, NULL) != 0)
                log_err("get service_property failed: %s", sub_node.c_str());
            else
                srv.properties_[sub_path[i]] = value;
        }
        else if (tp == PathType::kNode)
        {
            std::string department; std::string service; std::string node_p;
            if (!NodeType::node_parse(sub_node.c_str(), department, service, node_p)) {
                log_err("invalid node path: %s, we will ignore this node", sub_node.c_str());
                continue;
            }

            NodeType node(department, service, node_p);
            if (subscribe_node(node) != 0) {
                log_err("subscribe node %s faild!", sub_node.c_str());
                continue;
            }

            log_debug("successfully detected node %s", sub_node.c_str());
            srv.nodes_[node_p] = node;
        }
        else
        {
            // 其他类型节点？
            log_err("unhandled service sub path: %s", sub_node.c_str());
        }
    }

    {
        // 将监听的服务登记到本地的sub_services_中去
        std::lock_guard<std::mutex> lock(lock_);

        log_debug("successfully add/update service %s", service_path.c_str());
        (*sub_services_)[service_path] = srv;
    }

    return 0;
}

int zkFrame::subscribe_service(const std::string& department, const std::string& service) {

    uint32_t strategy = kStrategyDefault;

    {
        std::lock_guard<std::mutex> lock(lock_);
        auto iter = sub_services_->find(zkPath::make_path(department, service));
        if (iter != sub_services_->end())
            strategy = iter->second.pick_strategy_;
    }

    return subscribe_service(department, service, strategy);
}

int zkFrame::subscribe_node(NodeType& node) {

    std::string node_path = zkPath::make_path(node.department_, node.service_, node.node_);
    std::string value;

    if (client_->zk_get(node_path.c_str(), value, 1, NULL) != 0) {
        log_err("get node %s failed.", node_path.c_str());
        return -1;
    }

    std::vector<std::string> sub_path {};
    int code = client_->zk_get_children(node_path.c_str(), 1, sub_path);
    if (code != 0) {
        log_err("get service children node failed %d", code);
        return -1;
    }


    for (size_t i=0; i<sub_path.size(); ++i) {

        std::string sub_node = node_path + "/" + sub_path[i];
        PathType tp = zkPath::guess_path_type(sub_node);
        if (tp == PathType::kNodeProperty)
        {
            if( client_->zk_get(sub_node.c_str(), value, 1, NULL) != 0) {
                log_err("get service_property failed: %s", sub_node.c_str());
            }

            // 特殊的属性值处理
            if (sub_path[i] == "active") {
                node.active_ = (value == "1" ? true : false);
            } else if(sub_path[i] == "weight") {
                int weight = ::atoi(value.c_str());
                if (weight >= kWPMin && weight <= kWPMax)
                    node.weight_ = weight;
            } else if(sub_path[i] == "priority") {
                int priority = ::atoi(value.c_str());
                if (priority >= kWPMin && priority <= kWPMax)
                    node.priority_ = priority;
            } else if(sub_path[i] == "idc") {
                if (value != "")
                    node.idc_ = value;
            } else {
                node.properties_[sub_path[i]] = value;
            }
        }
        else
        {
            log_err("unhandled path: %s", sub_node.c_str());
        }
    }

    return 0;
}


int zkFrame::subscribe_node(const char* node_path) {

    if (!node_path || strlen(node_path) == 0)
        return -1;

    std::string department; std::string service; std::string node_p;
    if (!NodeType::node_parse(node_path, department, service, node_p)) {
        log_err("invalid node path: %s, we will ignore this node", node_path);
        return -1;
    }

    NodeType node(department, service, node_p);
    if (subscribe_node(node) != 0) {
        log_err("subscribe node %s faild!", node_path);
        return -1;
    }

    log_debug("successfully detected node %s", node_path);

    {
        std::lock_guard<std::mutex> lock(lock_);
        std::string service_path = zkPath::make_path(department, service);
        auto iter = sub_services_->find(service_path);
        if (iter != sub_services_->end()) {
            iter->second.nodes_[node_p] = node;
            log_debug("node %s register successfully.", node_path);
        } else {
            log_err("service %s not found, not subsubscribed??", service.c_str());
            return -1;
        }
    }

    return 0;
}


int zkFrame::pick_service_node(const std::string& department, const std::string& service,
                               NodeType& node) {
    return 0;
}


int zkFrame::handle_zk_event(int type, int state, const char* path) {

    assert(type != ZOO_SESSION_EVENT);

    if (!path || strlen(path) == 0) {
        log_err("can not handle with empty path, info: %d, %d", type, state);
        return -1;
    }

    PathType tp = zkPath::guess_path_type(path);
    int code = 0;
    switch (tp) {
        case PathType::kService:
            code = do_handle_zk_service_event(type, path);
            break;
        case PathType::kServiceProperty:
            code = do_handle_zk_service_properties_event(type, path);
            break;
        case PathType::kNode:
            code = do_handle_zk_node_event(type, path);
            break;
        case PathType::kNodeProperty:
            code = do_handle_zk_node_properties_event(type, path);
            break;

        default:
            log_err("unhandled path: %s", path);
            code = -1;
    }

    return code;
}

////
// 通知事件描述
// ZOO_CREATED_EVENT 节点创建事件，需要watch一个不存在的节点，当节点被创建时触发，通过zoo_exists()设置
// ZOO_DELETED_EVENT 节点删除事件，此watch通过zoo_exists()或zoo_get()设置
// ZOO_CHANGED_EVENT 节点数据改变事件，此watch通过zoo_exists()或zoo_get()设置
// ZOO_CHILD_EVENT   子节点列表改变事件，此watch通过zoo_get_children()或zoo_get_children2()设置
//
// ZOO_SESSION_EVENT 会话失效事件，客户端与服务端断开或重连时触发
// ZOO_NOTWATCHING_EVENT watch移除事件，服务端出于某些原因不再为客户端watch节点时触发
//

int zkFrame::do_handle_zk_service_event(int type, const char* service_path) {

    std::string department; std::string service;
    if(!ServiceType::service_parse(service_path, department, service)) {
        log_err("invalid service path provide: %s, event %s", service_path, zkClient::zevent_str(type));
        return -1;
    }

    if(type == ZOO_CREATED_EVENT) {
        // 服务重新上线，只需要再次监听就可以
        log_debug("re-sub_service %s", service_path);
        return subscribe_service(department, service);
    } else if(type == ZOO_DELETED_EVENT) {
        // 正常情况不应该删除服务目录节点的，这里会从监听的服务列表中删除
        // 该服务的注册信息，然后使用exists监听等待服务再次注册
        log_err("should not delete service normally, path %s !!!", service_path);

        {
            std::lock_guard<std::mutex> lock(lock_);
            auto iter = sub_services_->find(service_path);
            if (iter != sub_services_->end()) {
                log_info("delete service %s from subscribed list.", service_path);
                sub_services_->erase(service_path);
            } else {
                log_err("service %s not subscribed ??", service_path);
            }
        }

        // for ZOO_CREATED_EVENT
        client_->zk_exists(service_path, 1, NULL);
        return 0;
    } else if(type == ZOO_CHANGED_EVENT) {
        // 处理服务启动、禁用设置 == "1"
        std::string value;
        int code = 0;
        if( client_->zk_get(service_path, value, 1, NULL) == 0) {
            std::lock_guard<std::mutex> lock(lock_);
            auto iter = sub_services_->find(service_path);
            if (iter != sub_services_->end()) {
                iter->second.properties_["enable"] = value;
                iter->second.enabled_ = (value == "1");
            } else {
                log_err("service %s not subscribed, why we get this event???",
                        service_path);
                code = -1;
            }
        } else {
            log_err("retrieve value for path %s failed!", service_path);
            code = -1;
        }
        return code;
    } else if(type == ZOO_CHILD_EVENT) {
        // recevied when add/remove new properties or node
        return subscribe_service(department, service);
    } else if(type == ZOO_SESSION_EVENT) {
        // Painic
        log_err("should not handle session_event in zkFrame here!");
        return -1;
    } else if(type == ZOO_NOTWATCHING_EVENT) {
        // rewatch this service
        return subscribe_service(department, service);
    }

    log_err("unhandled event %s, path %s", zkClient::zevent_str(type), service_path);
    return -1;
}

int zkFrame::do_handle_zk_service_properties_event(int type, const char* service_property_path) {

    std::string department; std::string service; std::string property;
    if(!ServiceType::service_property_parse(service_property_path, department, service, property)) {
        log_err("invalid service property path: %s", service_property_path);
        return -1;
    }

    if(type == ZOO_CREATED_EVENT) {
        // Panic
        log_err("should not receive event %s for %s",
                zkClient::zevent_str(type), service_property_path);
        return -1;
    } else if(type == ZOO_DELETED_EVENT) {
        // 服务目录内容的增加删除会得到 ZOO_CHILD_EVENT，在那边自动处理
        return 0;
    } else if(type == ZOO_CHANGED_EVENT) {
        // 普通的服务节点属性更新
        std::string value;
        std::string service_path = zkPath::make_path(department, service);
        int code = 0;
        if( client_->zk_get(service_property_path, value, 1, NULL) == 0) {
            std::lock_guard<std::mutex> lock(lock_);
            auto iter = sub_services_->find(service_path);
            if (iter != sub_services_->end()) {
                iter->second.properties_[property] = value;
            } else {
                log_err("service %s not subscribed, why we get this event???",
                        service_path.c_str());
                code = -1;
            }
        } else {
            log_err("retrieve value for path %s failed.", service_property_path);
            code = -1;
        }
        return code;
    } else if(type == ZOO_CHILD_EVENT) {
        // property should not have child path
        log_err("service_property should not have child path, and we should not watch it acitvely: %s",
                service_property_path);
        return -1;
    } else if(type == ZOO_SESSION_EVENT) {
        // Painic
        log_err("should not handle session_event in zkFrame here!");
        return -1;
    } else if(type == ZOO_NOTWATCHING_EVENT) {
        // rewatch this service
        return subscribe_service(department, service);
    }

    log_err("unhandled event %s, path %s", zkClient::zevent_str(type), service_property_path);
    return -1;
}

int zkFrame::do_handle_zk_node_event(int type, const char* node_path) {

    std::string department; std::string service; std::string node;
    if(!NodeType::node_parse(node_path, department, service, node)) {
        log_err("invalid node path: %s", node_path);
        return -1;
    }

    if(type == ZOO_CREATED_EVENT) {
        // Panic
        log_err("should not receive event %s for %s",
                zkClient::zevent_str(type), node_path);
        return -1;
    } else if(type == ZOO_DELETED_EVENT) {
        // service will recv ZOO_CHILD_EVENT and handle it
        return 0;
    } else if(type == ZOO_CHANGED_EVENT) {
        // 节点启用禁用
        std::string value;
        std::string service_path = zkPath::make_path(department, service);
        int code = 0;
        if( client_->zk_get(node_path, value, 1, NULL) == 0) {
            std::lock_guard<std::mutex> lock(lock_);
            auto iter = sub_services_->find(service_path);
            if (iter != sub_services_->end()) {
                auto node_p = iter->second.nodes_.find(node);
                if (node_p != iter->second.nodes_.end()) {
                    node_p->second.properties_["enable"] = value;
                    node_p->second.enabled_ = (value == "1");
                } else {
                    log_err("node %s not found in sub_service, why we get this event?",
                            node.c_str());
                    code = -1;
                }
            } else {
                log_err("service %s not subscribed, why we get this event???", service_path.c_str());
                log_err("full nodes info for %s info:\n %s",
                        service_path.c_str(), iter->second.str().c_str());
                code = -1;
            }
        } else {
            log_err("retrieve value for path %s failed.", node_path);
            code = -1;
        }
        return code;
    } else if(type == ZOO_CHILD_EVENT) {
        // When add/remove new properties for node
        return subscribe_node(node_path);
    } else if(type == ZOO_SESSION_EVENT) {
        // Painic
        log_err("should not handle session_event in zkFrame here!");
        return -1;
    } else if(type == ZOO_NOTWATCHING_EVENT) {
        // rewatch this node
        return subscribe_node(node_path);
    }

    log_err("unhandled event %s, path %s", zkClient::zevent_str(type), node_path);
    return -1;
}

int zkFrame::do_handle_zk_node_properties_event(int type, const char* node_property_path) {

    std::string department; std::string service;
    std::string node; std::string property;
    if(!NodeType::node_property_parse(node_property_path,
                                      department, service, node, property)) {
        log_err("invalid node property path: %s", node_property_path);
        return -1;
    }

    if(type == ZOO_CREATED_EVENT) {
        // Panic
        log_err("should not receive event %s for %s",
                zkClient::zevent_str(type), node_property_path);
        return -1;
    } else if(type == ZOO_DELETED_EVENT) {
        // 节点目录会得到 ZOO_CHILD_EVENT，在那边处理
        return 0;
    } else if(type == ZOO_CHANGED_EVENT) {
        std::string value;
        std::string service_path = zkPath::make_path(department, service);
        int code = 0;
        if( client_->zk_get(node_property_path, value, 1, NULL) == 0) {
            std::lock_guard<std::mutex> lock(lock_);
            auto iter = sub_services_->find(service_path);
            if (iter != sub_services_->end()) {
                auto node_p = iter->second.nodes_.find(node);
                if (node_p != iter->second.nodes_.end()) {
                    node_p->second.properties_[property] = value;
                } else {
                    log_err("node %s not found in sub_service, why we get this event?",
                            node.c_str());
                    log_err("full nodes info for %s info:\n %s",
                            service_path.c_str(), iter->second.str().c_str());
                    code = -1;
                }
            } else {
                log_err("service %s not subscribed, why we get this event???", service_path.c_str());
                code = -1;
            }
        } else {
            log_err("retrieve value for path %s failed.", node_property_path);
            code = -1;
        }
        return code;
    } else if(type == ZOO_CHILD_EVENT) {
        // property should not have child path
        log_err("node_property should not have child path, and we should not watch it acitvely: %s",
                node_property_path);
        return -1;
    } else if(type == ZOO_SESSION_EVENT) {
        // Painic
        log_err("should not handle session_event in zkFrame here!");
        return -1;
    } else if(type == ZOO_NOTWATCHING_EVENT) {
        // rewatch this service
        return subscribe_node(zkPath::make_path(department, service, node).c_str());
    }

    log_err("unhandled event %s, path %s", zkClient::zevent_str(type), node_property_path);
    return -1;
}


// internal
// 根据0.0.0.0扩充得到实体节点
bool zkFrame::extend_substant_node(const NodeType& node, std::vector<NodeType>& nodes) {

    if (node.department_.empty() || node.service_.empty() || !zkPath::validate_node(node.node_)) {
        log_err("invalid Node parameter provide.");
        return false;
    }

    std::string host = node.node_.substr(0, node.node_.find(":"));
    std::string port = node.node_.substr(node.node_.find(":") + 1);
    nodes.clear();

    if (host != "0.0.0.0") {
        NodeType n_node = node;
        n_node.idc_ = idc_;
        n_node.host_ = host;
        n_node.port_ = ::atoi(port.c_str());
        nodes.push_back(n_node);
        return true;
    }

    // 添加本地实际的物理地址
    auto local_ips = zkPath::get_local_ips();
    for (size_t i=0; i<local_ips.size(); ++i) {
        NodeType n_node = node;
        n_node.node_ = local_ips[i] + ":" + port;
        n_node.idc_ = idc_;
        n_node.host_ = local_ips[i];
        n_node.port_ = ::atoi(port.c_str());
        nodes.push_back(n_node);
    }

    return !nodes.empty();
}




} // Clotho

