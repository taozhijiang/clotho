#include <chrono>
#include <zookeeper/zookeeper.h>

#include "zkFrame.h"
#include "zkRecipe.h"


namespace Clotho {

// zkRecipe主要是由zkFrame中的调用转发过来的，zkFrame负责进行参数校验检查

int zkRecipe::attach_node_property_cb(const std::string& dept, const std::string& service, const std::string& node,
                                      const PropertyCall& func) {

    std::string path = zkPath::make_path(dept, service, node);

    std::lock_guard<std::mutex> lock(node_lock_);
    path = zkPath::normalize_path(path);
    PathType pt = zkPath::guess_path_type(path);
    if (func && pt == PathType::kNode) {
        node_property_callmap_[path] = func;
        return 0;
    }

    return -1;
}

int zkRecipe::hook_node_calls(const std::string& full, const std::map<std::string, std::string>& properties) {
    
    int code = 0;
    PropertyCall func;

    {
        std::lock_guard<std::mutex> lock(node_lock_);
        auto iter = node_property_callmap_.find(full);
        if (iter != node_property_callmap_.end())
            func = iter->second;
    }

    if (func)
        code = func(full.c_str(), properties);
    
    return code;
}


int zkRecipe::hook_service_calls(const std::string& full, const std::map<std::string, std::string>& properties) {
    
    bool updated = false;
    {
        std::lock_guard<std::mutex> lock(serv_lock_);
        auto iter = serv_properties_.find(full);
        if (iter != serv_properties_.end()) {
            iter->second = properties;
            updated = true;
        }
    }

    if(updated)
        serv_notify_.notify_all();

    return 0;
}


bool zkRecipe::service_try_lock(const std::string& dept, const std::string& service, const std::string& lock_name,
                                const std::string& expect, uint32_t sec) {
    
    std::string serv_path = zkPath::make_path(dept, service);
    std::string lock_path = zkPath::extend_property(serv_path, "lock_" + lock_name);

    // wait until sec ...
    std::unique_lock<std::mutex> lock(serv_lock_);

    auto iter = serv_properties_.find(serv_path);
    if(iter == serv_properties_.end())
        serv_properties_[serv_path] = std::map<std::string, std::string>();

    if(sec == 0)
        return try_ephemeral_path_holder(lock_path, expect);

    auto now = std::chrono::system_clock::now();
    auto expire_tp = now + std::chrono::seconds(sec);

    // no_timeout wakeup by notify_all, notify_one, or spuriously
    // timeout    wakeup by timeout expiration
    while (!try_ephemeral_path_holder(lock_path, expect)) {

        // 如果超时，则直接到check处进行最后检查
        // 如果是伪唤醒，则还需要检查items_是否为空，如果是空则继续睡眠

#if __cplusplus >= 201103L
        if (serv_notify_.wait_until(lock, expire_tp) == std::cv_status::timeout) {
            break;
        }
#else
        if (!serv_notify_.wait_until(lock, expire_tp)) {
            break;
        }
#endif
    }

    if( try_ephemeral_path_holder(lock_path, expect) ) {
        serv_distr_locks_[lock_path] = expect;
        return true;
    }

    return false;
}

bool zkRecipe::service_lock(const std::string& dept, const std::string& service, const std::string& lock_name, const std::string& expect) {
    
    std::string serv_path = zkPath::make_path(dept, service);
    std::string lock_path = zkPath::extend_property(serv_path, "lock_" + lock_name);

    std::unique_lock<std::mutex> lock(serv_lock_);
    auto iter = serv_properties_.find(serv_path);
    if(iter == serv_properties_.end())
        serv_properties_[serv_path] = std::map<std::string, std::string>();


    while (!try_ephemeral_path_holder(lock_path, expect)) {
        serv_notify_.wait(lock);
    }

    serv_distr_locks_[lock_path] = expect;
    return true;
}


bool zkRecipe::service_unlock(const std::string& dept, const std::string& service, const std::string& lock_name, const std::string& expect) {
    
    std::string serv_path = zkPath::make_path(dept, service);
    std::string lock_path = zkPath::extend_property(serv_path, "lock_" + lock_name);

    std::unique_lock<std::mutex> lock(serv_lock_);

    std::string value;
    if(frame_.client_->zk_get(lock_path.c_str(), value, 1, NULL) != 0)
        return false;

    // we are the holder
    if(value == expect) {
        frame_.client_->zk_delete(lock_path.c_str());
        serv_distr_locks_.erase(lock_path);
        return true;
    }

    return false;
} 



bool zkRecipe::service_lock_owner(const std::string& dept, const std::string& service, const std::string& lock_name, const std::string& expect) {

    std::string serv_path = zkPath::make_path(dept, service);
    std::string lock_path = zkPath::extend_property(serv_path, "lock_" + lock_name);

    std::unique_lock<std::mutex> lock(serv_lock_);

    std::string value;
    if(frame_.client_->zk_get(lock_path.c_str(), value, 1, NULL) != 0)
        return false;

    // we are the holder
    if(value == expect) {
        serv_distr_locks_[lock_path] = expect;
        return true;
    }

    serv_distr_locks_[lock_path] = value;
    return false; 
}

bool zkRecipe::try_ephemeral_path_holder(const std::string& path, const std::string& expect) {

    if(frame_.client_->zk_create_if_nonexists(path.c_str(), expect, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL ) != 0)
        return false;

    std::string value;
    if(frame_.client_->zk_get(path.c_str(), value, 1, NULL) != 0)
        return false;

    return value == expect;
}


void zkRecipe::revoke_all_locks(const std::string& expect) {

    std::unique_lock<std::mutex> lock(serv_lock_);

    for(auto iter=serv_distr_locks_.begin(); iter != serv_distr_locks_.end(); ++iter) {
        
        std::string value;
        if(frame_.client_->zk_get(iter->first.c_str(), value, 1, NULL) != 0)
            continue;

        if(value == expect)
            frame_.client_->zk_delete(iter->first.c_str());
    }

    serv_distr_locks_.clear();

}

} // Clotho

