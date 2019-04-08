#include "zkFrame.h"


namespace tzkeeper {

#if 0
bool zkFrame::init(const std::string& hostline, const std::string& idc) {

    if (hostline.empty() || idc.empty()) {
        return false;
    }

    client_.reset(new zkClient(hostline, idc));
    if (!client_ || !client_->init()) {
        log_err("create and init zkClient failed.");
        client_.reset();
        return false;
    }
}

#endif

} // end namespace tzkeeper

