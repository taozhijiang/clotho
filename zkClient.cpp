#include <vector>
#include <zookeeper/zookeeper.h>

#include "zkClient.h"

#define CHECK_ZHANDLE(_zhandle_) do { \
    if(_zhandle_ == NULL || zoo_state(_zhandle_) != ZOO_CONNECTED_STATE) { \
        log_err("zhandle(%p) not valid. %d:%s.", _zhandle_, \
                      _zhandle_ != NULL ? zoo_state(_zhandle_) : -1, \
                      _zhandle_ != NULL ? zstate_str(zoo_state(_zhandle_)) : "STATE_NULL"); \
        return -1; \
    } \
} while(false);


namespace tzkeeper {

const static int ZOO_BUFFER_LEN = 1024;

static inline const char* zevent_str(int event) {
    if(event == ZOO_CREATED_EVENT) {
        return "ZOO_CREATED_EVENT";
    } else if(event == ZOO_DELETED_EVENT) {
        return "ZOO_DELETED_EVENT";
    } else if(event == ZOO_CHANGED_EVENT) {
        return "ZOO_CHANGED_EVENT";
    } else if(event == ZOO_CHILD_EVENT) {
        return "ZOO_CHILD_EVENT";
    } else if(event == ZOO_SESSION_EVENT) {
        return "ZOO_SESSION_EVENT";
    } else if(event == ZOO_NOTWATCHING_EVENT) {
        return "ZOO_NOTWATCHING_EVENT";
    } else {
        return "ZOO_UNKNOWN_EVENT";
    }
}

static inline const char* zstate_str(int state) {
    
    if(state == ZOO_EXPIRED_SESSION_STATE) {
        return "ZOO_EXPIRED_SESSION_STATE";
    } else if(state == ZOO_AUTH_FAILED_STATE) {
        return "ZOO_AUTH_FAILED_STATE";
    } else if(state == ZOO_CONNECTING_STATE) {
        return "ZOO_CONNECTING_STATE";
    } else if(state == ZOO_ASSOCIATING_STATE) {
        return "ZOO_ASSOCIATING_STATE";
    } else if(state == ZOO_CONNECTED_STATE) {
        return "ZOO_CONNECTED_STATE";
    } else {
        return "ZOO_UNKNOWN_STATE";
    }
}


int zkClient::zk_set(const char* path, const std::string& value, int version) {
   
    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    int ret = zoo_set(zhandle_, path, value.c_str(), value.size(), -1);
    if(ret < 0) {
        log_err("zoo_set %s:%s failed, ret: %s", path, value.c_str(), zerror(ret));
        return ret;
    }
    
    log_debug("zoo_set %s success. value: %s", path, value.c_str());
    return 0;
}

int zkClient::zk_get(const char* path, std::string& value, int watch, struct Stat* stat) {

    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    char szbuffer[ZOO_BUFFER_LEN] {};
    int buffer_len = ZOO_BUFFER_LEN;
    int ret = zoo_get(zhandle_, path, watch, szbuffer, &buffer_len, stat);
    if(ret < 0) {
        log_err("zoo_get %s failed, ret: %s", path, zerror(ret));
        return ret;
    }
    
    if(buffer_len < ZOO_BUFFER_LEN) {
        szbuffer[buffer_len] = '\0';
    } else {
        szbuffer[ZOO_BUFFER_LEN - 1] = '\0';
    }

    log_info("zoo_get %s success. value: %s, bufferlen: %d", path, szbuffer, buffer_len);
    value = szbuffer;
    return 0;
}

int zkClient::zk_exists(const char* path, int watch, struct Stat *stat) {
    
    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    int ret = zoo_exists(zhandle_, path, watch, stat);
    if(ret < 0) {
        log_err("zoo_exists %s failed, ret: %s", path, zerror(ret));
        return ret;
    }

    return 0;
}


int zkClient::zk_create(const char* path, const std::string& value, const struct ACL_vector *acl, int flags) {
    
    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    if(acl == NULL) {
        acl = &ZOO_OPEN_ACL_UNSAFE;
    }
    
    int ret = zoo_create(zhandle_, path, value.c_str(), value.size(), acl, flags, NULL, 0);
    if(ret < 0) {
        log_err("zoo_create %s failed, ret: %s", path, zerror(ret));
        return ret;
    }
    
    log_debug("zoo_create %s success, value: %s", path, value.c_str());
    return 0;
}

int zkClient::zk_delete(const char* path, int version) {
    
    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    int ret = zoo_delete(zhandle_, path, version);
    if(ret < 0) {
        log_err("zoo_delete %s failed, ret: %s", path, zerror(ret));
        return ret;
    }
    
    log_debug("zoo_delete %s success.", path);
    return 0;
}



int zkClient::zk_get_children(const char* path, int watch, std::vector<std::string>& children) {
    
    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    struct String_vector children_vec {};
    int ret = zoo_get_children(zhandle_, path, watch, &children_vec);
    if(ret < 0) {
        log_err("zoo_get_children %s failed, ret: %s", path, zerror(ret));
        return ret;
    }

    if(children_vec.count <= 0) {
        return 0;
    }

    children.reserve(children_vec.count);
    for(int index = 0; index < children_vec.count; index ++) {
        children.push_back(std::string(children_vec.data[index]));
    }
    deallocate_String_vector(&children_vec);

    return 0;
}

} // end namespace tzkeeper
