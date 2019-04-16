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

    return "";
}


int zkFrame::handle_zk_event(int type, int state, const char *path) {

    return 0;
}

} // Clotho

