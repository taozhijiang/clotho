#include "zkFrame.h"

#include <zookeeper/zookeeper.h>

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
    return 0;
}



int zkFrame::subscribe_service(const std::string& department, const std::string& service,
                               uint32_t strategy) {
    return 0;
}



int zkFrame::pick_service_node(const std::string& department, const std::string& service,
                               NodeType& node) {
    return 0;
}


int zkFrame::handle_zk_event(int type, int state, const char* path) {

    return 0;
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

