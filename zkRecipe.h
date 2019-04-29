#ifndef __CLOTHO_RECIPE_H__
#define __CLOTHO_RECIPE_H__


#include <mutex>
#include <condition_variable>

#include <map>
#include <string>

// zkFrame提供了基础的服务发布、发现方面的功能，而Recipe旨在提供
// 非核心的辅助功能，比如应用程序配置更新的回调、
// 服务下多节点的选主、分布式锁等功能

namespace Clotho {


class zkFrame;

typedef std::function<int(const std::string& notify,\
                          const std::map<std::string, std::string>& properties)> PropertyCall;
typedef std::map<std::string, std::string> StrMap;

class zkRecipe {

public:
    explicit zkRecipe(zkFrame& frame):
        frame_(frame) {}

    ~zkRecipe() = default;

    // 禁止拷贝
    zkRecipe(const zkRecipe&) = delete;
    zkRecipe& operator=(const zkRecipe&) = delete;

    
    // 注册制定路径的属性回调函数
    // 此处传入的路径只应该是服务节点，并且只有对应的服务被Watch了才有可能在属性变更的时候得到回调
    // 此处可能有点矛盾：即属性变更如果用作应用层的配置，那么应该是服务的发布节点
    //                   比较关注，但是sub应该是服务的消费节点比较关注的，所以如果
    //                   需要使用这个功能，那么服务的发布节点也需要sub服务才能实现对应功能
    int attach_node_property_cb(const std::string& dept, const std::string& service, const std::string& node,
                                  const PropertyCall& func);
                                  
    bool service_try_lock(const std::string& dept, const std::string& service, const std::string& lock_name, const std::string& expect, uint32_t sec);
    bool service_lock(const std::string& dept, const std::string& service, const std::string& lock_name, const std::string& expect);
    bool service_unlock(const std::string& dept, const std::string& service, const std::string& lock_name, const std::string& expect);
    bool service_lock_owner(const std::string& dept, const std::string& service, const std::string& lock_name, const std::string& expect);

    // 主动释放所有的分布式锁，加快其他节点抢占锁的时间
    void revoke_all_locks(const std::string& expect);


    int hook_node_calls(const std::string& full, const std::map<std::string, std::string>& properties);
    int hook_service_calls(const std::string& full, const std::map<std::string, std::string>& properties);

private:

    // 属性变更的回调列表
    std::mutex node_lock_;
    std::map<std::string, PropertyCall> node_property_callmap_;

    std::mutex serv_lock_;
    std::condition_variable serv_notify_;

    // 目前没有用到这里的properties_，主要是用key做唯一性检查，防止不关心的
    // 服务造成伪唤醒
    std::map<std::string, std::map<std::string, std::string>> serv_properties_;

    // 本地所注册的所有分布式锁实例
    std::map<std::string, std::string> serv_distr_locks_;

    bool try_ephemeral_path_holder(const std::string& path, const std::string& expect);

    zkFrame& frame_;
};

} // Clotho

#endif // __CLOTHO_RECIPE_H__

