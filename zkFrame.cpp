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

std::string zkFrame::make_full_path(const std::string& department,
                                    const std::string& service, const std::string& node) {

    if (department.empty() || service.empty() || !zkPath::validate_node(node)) {
        log_err("invalid param: %s, %s, %s", department.c_str(), service.c_str(), node.c_str());
        return "";
    }

    std::string path = "/" + department + "/" + service + "/" + node;
    return zkPath::normalize_path(path);
}

int zkFrame::prase_from_path(const char *path,
                             std::string& department, std::string& service, std::string& node) {
    if (!path) return -1;

    std::string str_path = zkPath::normalize_path(std::string(path));
    std::vector<std::string> items {};
    zkPath::split(str_path, "/", items);
    if (items.size() != 3 || !zkPath::validate_node(items[2])) {
        log_err("invalid path: %s", path);
        return -1;
    }

    department = items[0];
    service = items[0];
    node = items[0];

    return 0;
}


int zkFrame::handle_zk_event(int type, int state, const char *path) {

    return 0;
}

} // Clotho

