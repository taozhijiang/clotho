/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <chrono>
#include <zookeeper/zookeeper.h>

#include "zkFrame.h"
#include "zkRecipe.h"


namespace Clotho {

// zkRecipe主要是由zkFrame中的调用转发过来的，zkFrame负责进行参数校验检查

int zkRecipe::attach_node_property_cb(const std::string& dept, const std::string& service, const std::string& node,
                                      const NodePropertyCall& func) {

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


int zkRecipe::attach_serv_property_cb(const std::string& dept, const std::string& service,
                                      const ServPropertyCall& func) {
    std::string path = zkPath::make_path(dept, service);

    std::lock_guard<std::mutex> lock(node_lock_);
    path = zkPath::normalize_path(path);
    PathType pt = zkPath::guess_path_type(path);
    if (func && pt == PathType::kService) {
        serv_property_callmap_[path] = func;
        return 0;
    }

    return -1;
}

int zkRecipe::hook_node_calls(const std::string& dept, const std::string& serv, const std::string& node, const MapString& properties) {

    int code = 0;
    NodePropertyCall func;
    const std::string fullpath = zkPath::make_path(dept, serv, node);

    do {

        std::lock_guard<std::mutex> lock(node_lock_);

        // 首先检查properties是否真的修改了，因为周期性的检查机制，可能会导致该函数伪调用
        auto iter_p = node_properties_.find(fullpath);
        if (iter_p != node_properties_.end() && iter_p->second == properties)
            break;

        // 更新或者记录之
        node_properties_[fullpath] = properties;

        // 检查是否注册了用户回调函数
        auto iter_c = node_property_callmap_.find(fullpath);
        if (iter_c != node_property_callmap_.end())
            func = iter_c->second;

    } while (0);

    if (func)
        code = func(dept, serv, node, properties);

    return code;
}


int zkRecipe::hook_service_calls(const std::string& dept, const std::string& serv, const MapString& properties) {

    int code = 0;
    const std::string fullpath = zkPath::make_path(dept, serv);
    ServPropertyCall func;

    do {

        std::lock_guard<std::mutex> lock(serv_lock_);

        // 首先检查properties是否真的修改了，因为周期性的检查机制，可能会导致该函数伪调用
        auto iter_p = serv_properties_.find(fullpath);
        if (iter_p != serv_properties_.end() && iter_p->second == properties)
            break;

        serv_properties_[fullpath] = properties;

        auto iter_s = serv_property_callmap_.find(fullpath);
        if (iter_s != serv_property_callmap_.end())
            func = iter_s->second;

    } while (0);

    if (func)
        code = func(dept, serv, properties);

    // 看是否需要通知内部业务
    for (auto iter = serv_distr_locks_.begin(); iter != serv_distr_locks_.end(); ++iter) {
        if (::strncmp(iter->first.c_str(), fullpath.c_str(), fullpath.size()) == 0) {
            serv_notify_.notify_all();
            break;
        }
    }

    return code;
}


bool zkRecipe::service_try_lock(const std::string& dept, const std::string& service, const std::string& lock_name,
                                const std::string& expect, uint32_t sec) {

    std::string serv_path = zkPath::make_path(dept, service);
    std::string lock_path = zkPath::extend_property(serv_path, "lock_" + lock_name);

    // wait until sec ...
    std::unique_lock<std::mutex> lock(serv_lock_);

    serv_distr_locks_[lock_path] = "";

    auto iter = serv_properties_.find(serv_path);
    if (iter == serv_properties_.end())
        serv_properties_[serv_path] = { };

    // 非阻塞版本
    if (sec == 0) {
        if (try_ephemeral_path_holder(lock_path, expect)) {
            serv_distr_locks_[lock_path] = expect;
            return true;
        }

        return false;
    }

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

    if (try_ephemeral_path_holder(lock_path, expect)) {
        serv_distr_locks_[lock_path] = expect;
        return true;
    }

    return false;
}

bool zkRecipe::service_lock(const std::string& dept, const std::string& service, const std::string& lock_name,
                            const std::string& expect) {

    std::string serv_path = zkPath::make_path(dept, service);
    std::string lock_path = zkPath::extend_property(serv_path, "lock_" + lock_name);

    std::unique_lock<std::mutex> lock(serv_lock_);
    serv_distr_locks_[lock_path] = "";

    auto iter = serv_properties_.find(serv_path);
    if (iter == serv_properties_.end())
        serv_properties_[serv_path] = { };


    while (!try_ephemeral_path_holder(lock_path, expect)) {
        serv_notify_.wait(lock);
    }

    serv_distr_locks_[lock_path] = expect;
    return true;
}


bool zkRecipe::service_unlock(const std::string& dept, const std::string& service, const std::string& lock_name,
                              const std::string& expect) {

    std::string serv_path = zkPath::make_path(dept, service);
    std::string lock_path = zkPath::extend_property(serv_path, "lock_" + lock_name);

    std::unique_lock<std::mutex> lock(serv_lock_);

    std::string value;
    if (frame_.client_->zk_get(lock_path.c_str(), value, 1, NULL) != 0)
        return false;

    // we are the holder
    if (value == expect) {
        frame_.client_->zk_delete(lock_path.c_str());
        serv_distr_locks_.erase(lock_path);
        return true;
    }

    return false;
}



bool zkRecipe::service_lock_owner(const std::string& dept, const std::string& service, const std::string& lock_name,
                                  const std::string& expect) {

    std::string serv_path = zkPath::make_path(dept, service);
    std::string lock_path = zkPath::extend_property(serv_path, "lock_" + lock_name);

    std::unique_lock<std::mutex> lock(serv_lock_);

    std::string value;
    if (frame_.client_->zk_get(lock_path.c_str(), value, 1, NULL) != 0)
        return false;

    // we are the holder
    if (value == expect) {
        serv_distr_locks_[lock_path] = expect;
        return true;
    }

    serv_distr_locks_[lock_path] = value;
    return false;
}

bool zkRecipe::try_ephemeral_path_holder(const std::string& path, const std::string& expect) {

    if (frame_.client_->zk_create_if_nonexists(path.c_str(), expect, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL) != 0)
        return false;

    std::string value;
    if (frame_.client_->zk_get(path.c_str(), value, 1, NULL) != 0)
        return false;

    return value == expect;
}


void zkRecipe::revoke_all_locks(const std::string& expect) {

    std::unique_lock<std::mutex> lock(serv_lock_);

    for (auto iter = serv_distr_locks_.begin(); iter != serv_distr_locks_.end(); ++iter) {

        std::string value;
        if (frame_.client_->zk_get(iter->first.c_str(), value, 1, NULL) != 0)
            continue;

        if (value == expect)
            frame_.client_->zk_delete(iter->first.c_str());
    }

    serv_distr_locks_.clear();

}

} // Clotho

