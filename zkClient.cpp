#include <vector>
#include <thread>
#include <zookeeper/zookeeper.h>

#include "zkPath.h"
#include "zkClient.h"

#define CHECK_ZHANDLE(_zhandle_) do { \
    if(_zhandle_ == NULL || zoo_state(_zhandle_) != ZOO_CONNECTED_STATE) { \
        log_err("zhandle(%p) not valid. %d:%s.", _zhandle_, \
                      _zhandle_ != NULL ? zoo_state(_zhandle_) : -1, \
                      _zhandle_ != NULL ? zstate_str(zoo_state(_zhandle_)) : "STATE_NULL"); \
        return -1; \
    } \
} while(false);


namespace Clotho {

const static int ZOO_BUFFER_LEN = 4 * 1024;

const char* zkClient::zevent_str(int event) {

    if (event == ZOO_CREATED_EVENT) {
        return "ZOO_CREATED_EVENT";
    } else if (event == ZOO_DELETED_EVENT) {
        return "ZOO_DELETED_EVENT";
    } else if (event == ZOO_CHANGED_EVENT) {
        return "ZOO_CHANGED_EVENT";
    } else if (event == ZOO_CHILD_EVENT) {
        return "ZOO_CHILD_EVENT";
    } else if (event == ZOO_SESSION_EVENT) {
        return "ZOO_SESSION_EVENT";
    } else if (event == ZOO_NOTWATCHING_EVENT) {
        return "ZOO_NOTWATCHING_EVENT";
    } else {
        return "ZOO_UNKNOWN_EVENT";
    }
}

const char* zkClient::zstate_str(int state) {

    if (state == ZOO_EXPIRED_SESSION_STATE) {
        return "ZOO_EXPIRED_SESSION_STATE";
    } else if (state == ZOO_AUTH_FAILED_STATE) {
        return "ZOO_AUTH_FAILED_STATE";
    } else if (state == ZOO_CONNECTING_STATE) {
        return "ZOO_CONNECTING_STATE";
    } else if (state == ZOO_ASSOCIATING_STATE) {
        return "ZOO_ASSOCIATING_STATE";
    } else if (state == ZOO_CONNECTED_STATE) {
        return "ZOO_CONNECTED_STATE";
    } else {
        return "ZOO_UNKNOWN_STATE";
    }
}


zkClient::zkClient(const std::string& hostline, const BizEventFunc& func,
                   const std::string& idc, int session_timeout) :
    hostline_(hostline),
    idc_(idc),
    session_timeout_(session_timeout),
    biz_event_func_(func),
    zhandle_lock_(),
    zhandle_(NULL) {

    for (size_t i = 0; i < hostline_.size(); ++i) {
        if (hostline_[i] == ';')
            hostline_[i] = ',';
    }

    std::vector<std::string> host_vec{};
    zkPath::split(hostline_, ",", host_vec); // 比较混乱，基本, ;都有

    std::string n_hostline;
    for (size_t i = 0; i < host_vec.size(); ++i) {
        if (!zkPath::validate_node(host_vec[i])) {
            log_err("invalid host: %s", host_vec[i].c_str());
            hostline_ = "";
        }
    }
}

zkClient::~zkClient() {

    std::lock_guard<std::mutex> lock(zhandle_lock_);
    if (zhandle_) {
        zookeeper_close(zhandle_);
        zhandle_ = NULL;
    }
}


// 如果连接异常，就会持有zhandle_lock_的互斥锁，那么上层对client的任何
// 调用都会阻塞在此处
int zkClient::handle_session_event(int type, int state, const char* path) {

    std::lock_guard<std::mutex> lock(zhandle_lock_);

    if (state == ZOO_CONNECTING_STATE ||
        state == ZOO_ASSOCIATING_STATE) {
        return 0;
    }

    if (state != ZOO_CONNECTED_STATE) {
        while (true) {
            if (zk_init() == 0)
                break;

            log_err("try for ZooKeeper connecting...");
            ::sleep(5);
        }
    }
    return 0;
}

int zkClient::delegete_biz_event(int type, int state, const char* path) {
    if (biz_event_func_) {
        return biz_event_func_(type, state, path);
    }

    log_err("drop event with type %s, state %s, path %s", zevent_str(type), zstate_str(state), path);
    return 0;
}

static void
zkClient_watch_call(zhandle_t* zh, int type, int state, const char* path, void* watcher_ctx) {

    log_debug("event type %s, state %s, path %s",
              zkClient::zevent_str(type), zkClient::zstate_str(state), path);

    zkClient* zk = static_cast<zkClient*>(watcher_ctx);
    if (zk) {
        if (type == ZOO_SESSION_EVENT) {
            // 会话层的通知，zkClient处理
            zk->handle_session_event(type, state, path);
        } else {
            // 业务级别的事件通知，代理到zkFrame处理
            zk->delegete_biz_event(type, state, path);
        }
    }
}

bool zkClient::zk_init() {

    if (hostline_.empty() || session_timeout_ <= 0)
        return false;

    {
        std::lock_guard<std::mutex> lock(zhandle_lock_);

        if (zhandle_) {
            zookeeper_close(zhandle_);
            zhandle_ = NULL;
        }

        // zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);
        zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);

        zhandle_ = zookeeper_init(hostline_.c_str(), zkClient_watch_call, session_timeout_, NULL, this, 0);
        if (!zhandle_) {
            log_err("zookeeper_init failed. errno: %d:%s", errno, strerror(errno));
            return false;
        }

        // 同步等待，直到连接完成
        while (zoo_state(zhandle_) != ZOO_CONNECTED_STATE) {
            log_debug("wait zookeeper to be connectted. %d:%s", zoo_state(zhandle_), zstate_str(zoo_state(zhandle_)));
            ::usleep(50 * 1000);
        }
    }

#if 0
    // check service whether ok
    {
        const char* reserved_path = "/reserved_startup_check";
        zk_create(reserved_path, "startup_check_val", NULL, 0);

        std::string value {};
        auto ret = zk_get(reserved_path, value, 0, NULL);
        if (ret != 0 || value.empty()) {
            log_err("startup check failed.");
            return false;
        }
    }
#endif

    log_debug("zookeeper connect success.");
    return true;
}


int zkClient::zk_set(const char* path, const std::string& value, int version) {

    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    int ret = zoo_set(zhandle_, path, value.c_str(), value.size(), -1);
    if (ret < 0) {
        log_err("zoo_set %s:%s failed, ret: %s", path, value.c_str(), zerror(ret));
        return ret;
    }

    log_debug("zoo_set %s success. value: %s", path, value.c_str());
    return 0;
}

int zkClient::zk_get(const char* path, std::string& value, int watch, struct Stat* stat) {

    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    char szbuffer[ZOO_BUFFER_LEN]{};
    int buffer_len = ZOO_BUFFER_LEN;
    int ret = zoo_get(zhandle_, path, watch, szbuffer, &buffer_len, stat);
    if (ret < 0) {
        log_err("zoo_get %s failed, ret: %s", path, zerror(ret));
        return ret;
    }

    if (buffer_len < ZOO_BUFFER_LEN) {
        szbuffer[buffer_len] = '\0';
    } else {
        szbuffer[ZOO_BUFFER_LEN - 1] = '\0';
    }

    log_info("zoo_get %s success. value: %s, bufferlen: %d", path, szbuffer, buffer_len);
    value = szbuffer;
    return 0;
}

int zkClient::zk_exists(const char* path, int watch, struct Stat* stat) {

    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    int ret = zoo_exists(zhandle_, path, watch, stat);
    if (ret < 0) {
        if (ret == ZNONODE) // 不存在
            return 0;

        log_err("zoo_exists %s failed, ret: %s", path, zerror(ret));
        return ret;
    }

    return 1; // 存在
}


int zkClient::zk_create(const char* path, const std::string& value, const struct ACL_vector* acl, int flags) {

    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    if (acl == NULL) {
        acl = &ZOO_OPEN_ACL_UNSAFE;
    }

    int ret = zoo_create(zhandle_, path, value.c_str(), value.size(), acl, flags, NULL, 0);
    if (ret < 0) {
        if (ret ==  ZNODEEXISTS) {
            log_info("path %s already exists!", path);
        }

        log_err("zoo_create %s failed, ret: %s", path, zerror(ret));
        return ret;
    }

    log_debug("zoo_create %s success, value: %s", path, value.c_str());
    return 0;
}

int zkClient::zk_create_if_nonexists(const char* path, const std::string& value, const struct ACL_vector* acl, int flags) {
    int ret = zk_create(path, value, acl, flags);
    if (ret == ZNODEEXISTS)
        return 0;

    return ret;
}

int zkClient::zk_create_or_update(const char* path, const std::string& value, const struct ACL_vector* acl, int flags) {
    int ret = zk_create(path, value, acl, flags);
    if (ret == ZNODEEXISTS)
        return zk_set(path, value);

    return ret;
}

int zkClient::zk_delete(const char* path, int version) {

    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    int ret = zoo_delete(zhandle_, path, version);
    if (ret < 0) {
        log_err("zoo_delete %s failed, ret: %s", path, zerror(ret));
        return ret;
    }

    log_debug("zoo_delete %s success.", path);
    return 0;
}



int zkClient::zk_get_children(const char* path, int watch, std::vector<std::string>& children) {

    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    struct String_vector children_vec {
    };
    int ret = zoo_get_children(zhandle_, path, watch, &children_vec);
    if (ret < 0) {
        log_err("zoo_get_children %s failed, ret: %s", path, zerror(ret));
        return ret;
    }

    if (children_vec.count <= 0) {
        return 0;
    }

    children.reserve(children_vec.count);
    for (int index = 0; index < children_vec.count; index++) {
        children.push_back(std::string(children_vec.data[index]));
    }
    deallocate_String_vector(&children_vec);

    return 0;
}


int zkClient::zk_multi(int op_count, const zoo_op_t* ops, zoo_op_result_t* results) {

    std::lock_guard<std::mutex> lock(zhandle_lock_);
    CHECK_ZHANDLE(zhandle_);

    int ret = zoo_multi(zhandle_, op_count, ops, results);
    if (ret < 0) {
        log_err("zoo_multi failed, ret: %s, detail:", zerror(ret));
        for (int i = 0; i < op_count; i++)
            log_err("result for idx %d => err: %d:%s", i, results[i].err, zerror(results[i].err));

        return ret;
    }

    return 0;
}

} // Clotho
