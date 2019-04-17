#include "zkPath.h"
#include "zkFrame.h"


namespace Clotho {

bool zkFrame::init(const std::string& hostline, const std::string& idc) {

    if (hostline.empty() || idc.empty()) {
        return false;
    }

    auto func = std::bind(&zkFrame::handle_zk_event, this,
                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    client_.reset(new zkClient(hostline, func, idc));
    if (!client_ || !client_->zk_init()) {
        log_err("create and init zkClient failed.");
        client_.reset();
        return false;
    }

    return true;
}



int zkFrame::register_node(const NodeType& node) {

    if (node.department_.empty() || node.service_.empty() || !zkPath::validate_node(node.node_)) {
        log_err("invalid register parameters provide.");
        return -1;
    }

    // 添加本地实际的物理地址
    if (::strncmp(node.node_.c_str(), "0.0.0.0", strlen("0.0.0.0")) == 0) {}

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



} // Clotho

