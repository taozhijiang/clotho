#include <cassert>
#include <algorithm>
#include <zookeeper/zookeeper.h>

#include "zkFrame.h"

namespace Clotho {

zkFrame::zkFrame(const std::string& idc) :
    client_(),
    recipe_(),
    idc_(idc),
    primary_node_addr_(),
    whole_nodes_addr_(),
    lock_(),
    pub_nodes_(),
    sub_services_() {

    auto local_ips = zkPath::get_local_ips();
    if (local_ips.empty()) {
        log_err("Get LocalIps failed.");
        throw ConstructException("Get LocalIps failed.");
    }
    primary_node_addr_ = local_ips[0];

    for (size_t i = 0; i < local_ips.size(); ++i) {
        whole_nodes_addr_.emplace_back(local_ips[i]);
    }
}


// defined at zkClient.cpp
extern bool g_terminating_;

zkFrame::~zkFrame() {

    // 不再响应任何事件通知的处理
    g_terminating_ = true;

    std::string expect = primary_node_addr_ + "-" + Clotho::to_string(::getpid());
    recipe_->revoke_all_locks(expect);

    std::lock_guard<std::mutex> lock(lock_);
    client_.reset();
}


bool zkFrame::init(const std::string& hostline) {

    if (hostline.empty() || idc_.empty() ||
        whole_nodes_addr_.empty() || primary_node_addr_.empty()) {
        return false;
    }

    auto func = std::bind(&zkFrame::handle_zk_event, this,
                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    client_.reset(new zkClient(hostline, func, idc_));
    if (!client_ || !client_->zk_init()) {
        log_err("create and init zkClient failed.");
        client_.reset();
        return false;
    }

    recipe_.reset(new zkRecipe(*this));
    if (!recipe_) {
        log_err("create zkRecipe failed.");
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
    if (substitute_node(node, nodes) != 0) {
        log_err("substitue to real nodes failed.");
        return -1;
    }

    for (size_t i = 0; i < nodes.size(); ++i) {

        VectorPair paths{};
        if (!nodes[i].prepare_path(paths)) {
            log_err("prepare path for %s failed.", nodes[i].node_.c_str());
            continue;
        }

        for (auto iter = paths.begin(); iter != paths.end(); ++iter) {
            PathType tp = zkPath::guess_path_type(iter->first);
            if (tp == PathType::kDepartment || tp == PathType::kService || tp == PathType::kNode) {
                client_->zk_create_if_nonexists(iter->first.c_str(), iter->second, &ZOO_OPEN_ACL_UNSAFE, 0);
            } else if (tp == PathType::kServiceProperty || tp == PathType::kNodeProperty) {
                if (overwrite)
                    client_->zk_create_or_update(iter->first.c_str(), iter->second, &ZOO_OPEN_ACL_UNSAFE, 0);
                else
                    client_->zk_create_if_nonexists(iter->first.c_str(), iter->second, &ZOO_OPEN_ACL_UNSAFE, 0);
            } else {
                log_err("Unknown PathType for %s", iter->first.c_str());
            }
        }

        std::string full_node_path = zkPath::make_path(nodes[i].department_, nodes[i].service_, nodes[i].node_);

        // add additional active path, EPHEMERAL node here
        std::string active_path = zkPath::extend_property(full_node_path, "active");
        if (client_->zk_create(active_path.c_str(), "1", &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL) != 0) {
            log_err("Create EPHEMERAL active failed, cirital error: %s", active_path.c_str());
            continue;
        }

        // additional non-critial pid
        std::string pid_path = zkPath::extend_property(full_node_path, "pid");
        std::string pid_value = Clotho::to_string(::getpid());
        if (client_->zk_create(pid_path.c_str(), pid_value, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL) != 0) {
            log_err("Create EPHEMERAL pid %s failed.", pid_path.c_str());
        }

        {
            // 执行添加操作
            std::string full = zkPath::make_path(nodes[i].department_, nodes[i].service_, nodes[i].node_);
            full = zkPath::normalize_path(full);

            std::lock_guard<std::mutex> lock(lock_);
            (*pub_nodes_)[full] = nodes[i];

            log_debug("successfully add %s into pub_nodes_", full.c_str());
        }
    }

    return 0;
}


int zkFrame::revoke_node(const std::string& node_path) {

    PathType tp = zkPath::guess_path_type(node_path);
    if (tp != PathType::kNode) {
        log_err("invalid node path: %s", node_path.c_str());
        return -1;
    }

    std::shared_ptr<MapNodeType> reg_nodes;
    {
        std::lock_guard<std::mutex> lock(lock_);
        pub_nodes_->erase(node_path);
    }

    std::string active_path = zkPath::extend_property(node_path, "active");
    client_->zk_delete(active_path.c_str());

    // 不需要将节点置为禁用状态，我们通过active也能控制节点是否被启用了
    // client_->zk_set(node_path.c_str(), "0");

    return 0;
}

int zkFrame::revoke_all_nodes() {

    MapNodeType reg_nodes{};
    {
        std::lock_guard<std::mutex> lock(lock_);
        reg_nodes = *pub_nodes_; // 拷贝内容
    }

    for (auto iter = reg_nodes.begin(); iter != reg_nodes.end(); ++iter) {
        revoke_node(iter->first);
    }

    return 0;
}


// subscribe_service should also subscribe its node instance
// subscribe_service will update corresponding param and value with zk_get
int zkFrame::subscribe_service(const std::string& department, const std::string& service,
                               uint32_t strategy, bool with_nodes) {

    std::string service_path = zkPath::make_path(department, service);
    if (zkPath::guess_path_type(service_path) != PathType::kService) {
        log_err("invalid service path: %s", service_path.c_str());
        return -1;
    }

    ServiceType srv(department, service);

    std::string value;
    int code = client_->zk_get(service_path.c_str(), value, 1, NULL);
    if (code != 0) {
        log_err("get service %s failed.", service_path.c_str());
        return -1;
    }

    srv.properties_["enable"] = value;
    srv.enabled_ = (value == "1");
    srv.pick_strategy_ = strategy ? strategy : kStrategyDefault;
    srv.with_nodes_ = with_nodes;

    // 处理子节点
    std::vector<std::string> sub_path{};
    code = client_->zk_get_children(service_path.c_str(), 1, sub_path);
    if (code != 0) {
        log_err("get service children node failed %d", code);
        return -1;
    }

    for (size_t i = 0; i < sub_path.size(); ++i) {

        std::string sub_node = service_path + "/" + sub_path[i];
        PathType tp = zkPath::guess_path_type(sub_node);
        if (tp == PathType::kServiceProperty) {
            if (client_->zk_get(sub_node.c_str(), value, 1, NULL) != 0)
                log_err("get service_property failed: %s", sub_node.c_str());
            else
                srv.properties_[sub_path[i]] = value;
        } else if (tp == PathType::kNode) {

            // 不需要处理子节点
            if (!srv.with_nodes_)
                continue;

            std::string department;
            std::string service;
            std::string node_p;
            if (!NodeType::node_parse(sub_node.c_str(), department, service, node_p)) {
                log_err("invalid node path: %s, we will ignore this node", sub_node.c_str());
                continue;
            }

            NodeType node(department, service, node_p);
            if (internal_subscribe_node(node) != 0) {
                log_err("subscribe node %s faild!", sub_node.c_str());
                continue;
            }

            log_debug("successfully detect and subscribe node %s", sub_node.c_str());
            srv.nodes_[node_p] = node;
        } else {
            // 其他类型节点？
            log_err("unhandled service sub path: %s", sub_node.c_str());
        }
    }

    {
        // 将监听的服务登记到本地的sub_services_中去
        std::lock_guard<std::mutex> lock(lock_);

        log_debug("successfully add/update service %s", service_path.c_str());
        (*sub_services_)[service_path] = srv;
    }

    return 0;
}

// 保留之前的subscribe的策略
int zkFrame::internal_subscribe_service(const std::string& department, const std::string& service) {

    uint32_t strategy   = kStrategyDefault;
    bool     with_nodes = false;

    {
        std::lock_guard<std::mutex> lock(lock_);
        auto iter = sub_services_->find(zkPath::make_path(department, service));
        if (iter != sub_services_->end()) {
            strategy = iter->second.pick_strategy_;
            with_nodes = iter->second.with_nodes_;
        }
    }

    return subscribe_service(department, service, strategy, with_nodes);
}

int zkFrame::internal_subscribe_node(NodeType& node) {

    std::string node_path = zkPath::make_path(node.department_, node.service_, node.node_);
    std::string value;

    if (client_->zk_get(node_path.c_str(), value, 1, NULL) != 0) {
        log_err("get node %s failed.", node_path.c_str());
        return -1;
    }

    node.properties_["enable"] = value;
    node.enabled_ = (value == "1");

    if (!zkPath::validate_node(node.node_, node.host_, node.port_)) {
        log_err("validate nodename failed: %s", node.node_.c_str());
        return -1;
    }

    std::vector<std::string> sub_path{};
    int code = client_->zk_get_children(node_path.c_str(), 1, sub_path);
    if (code != 0) {
        log_err("get service children node failed %d", code);
        return -1;
    }

    for (size_t i = 0; i < sub_path.size(); ++i) {

        std::string sub_node = node_path + "/" + sub_path[i];
        PathType tp = zkPath::guess_path_type(sub_node);
        if (tp == PathType::kNodeProperty) {
            if (client_->zk_get(sub_node.c_str(), value, 1, NULL) != 0) {
                log_err("get service_property failed: %s", sub_node.c_str());
            }

            // 特殊的属性值处理
            // 这些类型的属性是不会丢到properties_中保存的
            if (sub_path[i] == "active") {
                node.active_ = (value == "1" ? true : false);
            } else if (sub_path[i] == "weight") {
                int weight = ::atoi(value.c_str());
                if (weight >= kWPMin && weight <= kWPMax)
                    node.weight_ = weight;
            } else if (sub_path[i] == "priority") {
                int priority = ::atoi(value.c_str());
                if (priority >= kWPMin && priority <= kWPMax)
                    node.priority_ = priority;
            } else if (sub_path[i] == "idc") {
                if (value != "")
                    node.idc_ = value;
            }

            // all will be recorded in properties_
            node.properties_[sub_path[i]] = value;

        } else {
            log_err("unhandled path: %s", sub_node.c_str());
        }
    }

    return 0;
}


int zkFrame::internal_subscribe_node(const char* node_path) {

    if (!node_path || strlen(node_path) == 0)
        return -1;

    std::string department;
    std::string service;
    std::string node_p;
    if (!NodeType::node_parse(node_path, department, service, node_p)) {
        log_err("invalid node path: %s, we will ignore this node", node_path);
        return -1;
    }

    NodeType node(department, service, node_p);
    if (internal_subscribe_node(node) != 0) {
        log_err("subscribe node %s faild!", node_path);
        return -1;
    }

    log_debug("successfully detected node %s", node_path);

    {
        std::lock_guard<std::mutex> lock(lock_);
        std::string service_path = zkPath::make_path(department, service);
        auto iter = sub_services_->find(service_path);
        if (iter != sub_services_->end()) {
            iter->second.nodes_[node_p] = node;
            log_debug("node %s register successfully.", node_path);
        } else {
            log_err("service %s not found, not subsubscribed??", service.c_str());
            return -1;
        }
    }

    return 0;
}


int zkFrame::pick_service_node(const std::string& department, const std::string& service,
                               NodeType& node) {

    uint32_t strategy = 0;
    std::string service_path = zkPath::make_path(department, service);

    {
        std::lock_guard<std::mutex> lock(lock_);
        auto iter = sub_services_->find(service_path);
        if (iter == sub_services_->end()) {
            log_err("can not find %s in sub_service!", service_path.c_str());
            return -1;
        }

        strategy = iter->second.pick_strategy_;
    }

    return pick_service_node(department, service, strategy, node);
}

// 降序方式排列优先级
static inline int sort_node_by_priority(const NodeType& n1, const NodeType& n2) {
    return (n1.priority_ > n2.priority_);
}

int zkFrame::pick_service_node(const std::string& department, const std::string& service,
                               uint32_t strategy, NodeType& node) {

    static uint32_t CHOOSE_INDEX = 0;

    std::string service_path = zkPath::make_path(department, service);
    if (zkPath::guess_path_type(service_path) != PathType::kService || strategy == 0) {
        log_err("pick service arguments error: %s, %d", service_path.c_str(), strategy);
        return -1;
    }

    ServiceType service_instance{};

    {
        std::lock_guard<std::mutex> lock(lock_);
        auto iter = sub_services_->find(service_path);
        if (iter == sub_services_->end()) {
            log_err("can not find %s in sub_service!", service_path.c_str());
            return -1;
        }

        // 首先拷贝，后续考虑优化，尤其对于这种读多写少的数据
        service_instance = iter->second; // copy
    }

    std::vector<NodeType> before{};
    std::vector<NodeType> filtered{};

    // Step0. 选取所有可用节点
    for (auto iter = service_instance.nodes_.begin();
         iter != service_instance.nodes_.end();
         ++iter) {
        if (iter->second.available())
            before.emplace_back(iter->second);
    }

    if (before.empty()) {
        log_err("not any available nodes for service %s with avaiable check.", service_path.c_str());
        return -1;
    }


    // Step1. 如果有kStragetyMaster，则选择Master节点；失败就返回
    if (strategy & kStrategyMaster) {
        auto iter = service_instance.properties_.find("lock_master");
        if (iter != service_instance.properties_.end()) {

            std::string str_node_pid = iter->second;

            // 设计原因，lock节点存储的是ip-pid的数据来标识锁的隶属的，我们无法保证存储节点信息，因为
            // 非注册的节点也可以尝试获取分布式锁
            // 这里根据每个节点properties的pid属性来进行尝试匹配
            for (size_t i = 0; i < before.size(); ++i) {
                if (before[i].properties_.find("pid") == before[i].properties_.end())
                    continue;
                std::string expect = before[i].host_ + "-" + before[i].properties_["pid"];
                if (expect == str_node_pid) {
                    node = before[i];
                    return 0;
                }
            }

            log_err("available master node %s not found", str_node_pid.c_str());
            return -1;
        }

        log_err("lock_master not found for service /%s/%s", department.c_str(), service.c_str());
        return -1;
    }


    // Step2. 根据IDC进行候选解点的筛选
    filtered.clear();
    if (strategy & kStrategyIdc) {
        for (size_t i = 0; i < before.size(); ++i) {
            if (before[i].idc_ == idc_)
                filtered.emplace_back(before[i]);
        }

        // 如果只得到一个可用节点，就直接返回这个节点
        if (filtered.size() == 1) {
            node = filtered[0];
            return 0;
        }

        // 如果IDC筛选后可用节点为空，则取消IDC筛选条件
        if (filtered.empty()) {
            log_info("filtered by kStrategyIdc remains empty nodes, reset IDC strict.");
        } else {
            before = filtered;
        }
    }

    // Step3. 随机选择可用节点
    if (strategy & kStrategyRandom) {
        uint32_t rands = static_cast<uint32_t>(::random());
        node = before[rands % before.size()];
        log_debug("by kStrategyRandom, return %s",
                  zkPath::make_path(node.department_, node.service_, node.node_).c_str());
        return 0;
    }

    // Step4. Round-Robin方式轮询
    if (strategy & kStrategyRoundRobin) {
        if (++CHOOSE_INDEX > 0xFFFF)
            CHOOSE_INDEX = 0;
        node = before[CHOOSE_INDEX % before.size()];
        log_debug("by kStrategyRoundRoubin, return %s",
                  zkPath::make_path(node.department_, node.service_, node.node_).c_str());
        return 0;
    }

    // Step5. 默认的，根据优先级和权重的方式筛选
    filtered.clear();
    if (strategy & kStrategyWP) {
        std::sort(before.begin(), before.end(), sort_node_by_priority);
    }

    uint32_t top_priority = before[0].priority_;
    uint32_t total_weight = 0;
    std::vector<uint32_t> weight_ladder;

    for (size_t i = 0; i < before.size(); ++i) {
        if (before[i].priority_ < top_priority)
            break;

        total_weight += before[i].weight_;
        weight_ladder.push_back(total_weight);
    }

    uint32_t rand_w = static_cast<uint32_t>(::random() % total_weight);
    for (size_t i = 0; i < weight_ladder.size(); ++i) {
        if (rand_w <= weight_ladder[i]) {
            node = before[i];
            log_debug("filter by priority and weight, return %s",
                      zkPath::make_path(node.department_, node.service_, node.node_).c_str());
            return 0;
        }
    }

    log_err("no available node, or your algorithm's problem.");
    return -1;
}


int zkFrame::periodicly_care() {

    std::vector<std::string> services{};

    {
        std::lock_guard<std::mutex> lock(lock_);
        for (auto iter = sub_services_->begin(); iter != sub_services_->end(); ++iter)
            services.push_back(iter->first);
    }

    for (size_t i = 0; i < services.size(); ++i) {
        std::string depart;
        std::string service;
        if (ServiceType::service_parse(services[i].c_str(), depart, service))
            internal_subscribe_service(depart, service);
    }

    return 0;
}



int zkFrame::set_priority(NodeType& node, uint16_t priority) {
    uint16_t original = node.priority_;

    node.priority_ = priority;
    return original;
}

int zkFrame::adj_priority(NodeType& node, int16_t step) {
    uint16_t original = node.priority_;

    int16_t total = node.priority_ + step;
    total = total < kWPMin ? kWPMin : total;
    total = total > kWPMax ? kWPMax : total;
    node.priority_ = static_cast<uint16_t>(total);
    return original;
}

int zkFrame::set_weight(NodeType& node, uint16_t weight) {
    uint16_t original = node.weight_;

    node.weight_ = weight;
    return original;
}

int zkFrame::adj_weight(NodeType& node, int16_t step) {
    uint16_t original = node.weight_;

    int16_t total = node.weight_ + step;
    total = total < kWPMin ? kWPMin : total;
    total = total > kWPMax ? kWPMax : total;
    node.weight_ = static_cast<uint16_t>(total);
    return original;
}

int zkFrame::recipe_attach_node_property_cb(const std::string& dept, const std::string& service, const std::string& node,
                                            const NodePropertyCall& func) {


    if (dept.empty() || service.empty() || !zkPath::validate_node(node) || !func) {
        log_err("invalid node path params.");
        return -1;
    }

    // property_cb 的注册，必须要注册with_nodes
    uint32_t strategy   = kStrategyDefault;

    {
        std::lock_guard<std::mutex> lock(lock_);
        auto iter = sub_services_->find(zkPath::make_path(dept, service));
        if (iter != sub_services_->end()) {
            strategy = iter->second.pick_strategy_;
        }
    }


    // 前提是先注册服务的监听，然后再调用recipe注册func
    int code = subscribe_service(dept, service, strategy, true);
    if (code != 0) {
        log_err("subscribe service /%s/%s failed.", dept.c_str(), service.c_str());
        return code;
    }

    return recipe_->attach_node_property_cb(dept, service, node, func);
}

int zkFrame::recipe_attach_serv_property_cb(const std::string& dept, const std::string& service, const ServPropertyCall& func) {


    if (dept.empty() || service.empty() || !func) {
        log_err("invalid node path params.");
        return -1;
    }


    // 前提是先注册服务的监听，然后再调用recipe注册func
    int code = internal_subscribe_service(dept, service);
    if (code != 0) {
        log_err("subscribe service /%s/%s failed.", dept.c_str(), service.c_str());
        return code;
    }

    return recipe_->attach_serv_property_cb(dept, service, func);
}

bool zkFrame::recipe_service_try_lock(const std::string& dept, const std::string& service, const std::string& lock_name, uint32_t sec) {

    if (dept.empty() || service.empty() || lock_name.empty()) {
        log_err("invalid service path params.");
        return -1;
    }

    // 前提是先注册服务的监听，然后再调用recipe注册func
    int code = internal_subscribe_service(dept, service);
    if (code != 0) {
        log_err("subscribe service /%s/%s failed.", dept.c_str(), service.c_str());
        return code;
    }


    std::string expect = primary_node_addr_ + "-" + Clotho::to_string(::getpid());
    return recipe_->service_try_lock(dept, service, lock_name, expect, sec);
}

bool zkFrame::recipe_service_lock(const std::string& dept, const std::string& service, const std::string& lock_name) {

    if (dept.empty() || service.empty() || lock_name.empty()) {
        log_err("invalid service path params.");
        return -1;
    }

    // 前提是先注册服务的监听，然后再调用recipe注册func
    int code = internal_subscribe_service(dept, service);
    if (code != 0) {
        log_err("subscribe service /%s/%s failed.", dept.c_str(), service.c_str());
        return code;
    }

    std::string expect = primary_node_addr_ + "-" + Clotho::to_string(::getpid());
    return recipe_->service_lock(dept, service, lock_name, expect);
}

bool zkFrame::recipe_service_unlock(const std::string& dept, const std::string& service, const std::string& lock_name) {

    if (dept.empty() || service.empty() || lock_name.empty()) {
        log_err("invalid service path params.");
        return -1;
    }

    // 前提是先注册服务的监听，然后再调用recipe注册func
    int code = internal_subscribe_service(dept, service);
    if (code != 0) {
        log_err("subscribe service /%s/%s failed.", dept.c_str(), service.c_str());
        return code;
    }

    std::string expect = primary_node_addr_ + "-" + Clotho::to_string(::getpid());
    return recipe_->service_unlock(dept, service, lock_name, expect);
}

bool zkFrame::recipe_service_lock_owner(const std::string& dept, const std::string& service, const std::string& lock_name) {

    if (dept.empty() || service.empty() || lock_name.empty()) {
        log_err("invalid service path params.");
        return false;
    }

    std::string expect = primary_node_addr_ + "-" + Clotho::to_string(::getpid());
    return recipe_->service_lock_owner(dept, service, lock_name, expect);
}



// 受限安全使用
static inline std::string base_path(const std::string& path) {
    if (path.empty())
        return "";

    return path.substr(0, path.find_last_of('/'));
}

int zkFrame::handle_zk_event(int type, int state, const char* path) {

    assert(type != ZOO_SESSION_EVENT);

    if (!path || strlen(path) == 0) {
        log_err("can not handle with empty path, info: %d, %d", type, state);
        return -1;
    }

    PathType tp = zkPath::guess_path_type(path);
    int code = 0;
    switch (tp) {
        case PathType::kService:
            code = internal_handle_zk_service_event(type, path);
            break;
        case PathType::kServiceProperty:
            code = internal_handle_zk_service_properties_event(type, path);
            break;
        case PathType::kNode:
            code = internal_handle_zk_node_event(type, path);
            break;
        case PathType::kNodeProperty:
            code = internal_handle_zk_node_properties_event(type, path);
            break;

        default:
            log_err("unhandled path: %s", path);
            code = -1;
    }

    // 检查是否需要回调property_cb

    std::string cb_serv_path;
    std::string cb_node_path;

    if (code == 0) {

        do {

            if (tp == PathType::kService &&
                (type == ZOO_CREATED_EVENT ||
                 type == ZOO_CHANGED_EVENT ||
                 type == ZOO_CHILD_EVENT ||
                 type == ZOO_NOTWATCHING_EVENT)) {
                cb_serv_path = path;
            } else if (tp == PathType::kServiceProperty &&
                       (type == ZOO_CHANGED_EVENT ||
                        type == ZOO_NOTWATCHING_EVENT)) {
                cb_serv_path = base_path(path);
            } else if (tp == PathType::kNode &&
                       (type == ZOO_CHANGED_EVENT ||
                        type == ZOO_CHILD_EVENT ||
                        type == ZOO_NOTWATCHING_EVENT)) {
                cb_node_path = path;
            } else if (tp == PathType::kNodeProperty &&
                       (type == ZOO_CHANGED_EVENT ||
                        type == ZOO_NOTWATCHING_EVENT)) {
                cb_node_path = base_path(path);
            }


            std::map<std::string, std::string> properties;
            if (!cb_serv_path.empty()) {

                std::string dept;
                std::string serv;
                if (!ServiceType::service_parse(cb_serv_path.c_str(), dept, serv)) {
                    break;
                }

                {
                    std::lock_guard<std::mutex> lock(lock_);
                    auto iter = sub_services_->find(cb_serv_path);
                    if (iter != sub_services_->end()) {
                        properties = iter->second.properties_;
                    }
                }

                if (!dept.empty() && !serv.empty() && !properties.empty()) {
                    code = recipe_->hook_service_calls(dept, serv, properties);
                }

            } else if (!cb_node_path.empty()) {

                std::string dept;
                std::string serv;
                std::string node;
                if (!NodeType::node_parse(cb_node_path.c_str(), dept, serv, node)) {
                    break;
                }

                {
                    std::lock_guard<std::mutex> lock(lock_);
                    auto iter = sub_services_->find(base_path(cb_node_path));
                    if (iter != sub_services_->end()) {
                        auto node_p = iter->second.nodes_.find(node);
                        if (node_p != iter->second.nodes_.end()) {
                            properties = node_p->second.properties_;
                        }
                    }
                }

                if (!dept.empty() && !serv.empty() && !node.empty() && !properties.empty()) {
                    code = recipe_->hook_node_calls(dept, serv, node, properties);
                }

            }

        } while (0);
    }

    return code;
}

////
// 通知事件描述
// ZOO_CREATED_EVENT 节点创建事件，需要watch一个不存在的节点，当节点被创建时触发，通过zoo_exists()设置
// ZOO_DELETED_EVENT 节点删除事件，此watch通过zoo_exists()或zoo_get()设置
// ZOO_CHANGED_EVENT 节点数据改变事件，此watch通过zoo_exists()或zoo_get()设置
// ZOO_CHILD_EVENT   子节点列表改变事件，此watch通过zoo_get_children()或zoo_get_children2()设置
//
// ZOO_SESSION_EVENT 会话失效事件，客户端与服务端断开或重连时触发
// ZOO_NOTWATCHING_EVENT watch移除事件，服务端出于某些原因不再为客户端watch节点时触发
//

int zkFrame::internal_handle_zk_service_event(int type, const char* service_path) {

    std::string department;
    std::string service;
    if (!ServiceType::service_parse(service_path, department, service)) {
        log_err("invalid service path provide: %s, event %s", service_path, zkClient::zevent_str(type));
        return -1;
    }

    if (type == ZOO_CREATED_EVENT) {
        // 服务重新上线，只需要再次监听就可以
        log_debug("re-sub_service %s", service_path);
        return internal_subscribe_service(department, service);
    } else if (type == ZOO_DELETED_EVENT) {
        // 正常情况不应该删除服务目录节点的，这里会从监听的服务列表中删除
        // 该服务的注册信息，然后使用exists监听等待服务再次注册
        log_err("should not delete service normally, path %s !!!", service_path);

        {
            std::lock_guard<std::mutex> lock(lock_);
            auto iter = sub_services_->find(service_path);
            if (iter != sub_services_->end()) {
                log_info("delete service %s from subscribed list.", service_path);
                sub_services_->erase(service_path);
            } else {
                log_err("service %s not subscribed ??", service_path);
            }
        }

        // for ZOO_CREATED_EVENT
        client_->zk_exists(service_path, 1, NULL);
        return 0;
    } else if (type == ZOO_CHANGED_EVENT) {
        // 处理服务启动、禁用设置 == "1"
        std::string value;
        int code = 0;
        if (client_->zk_get(service_path, value, 1, NULL) == 0) {
            std::lock_guard<std::mutex> lock(lock_);
            auto iter = sub_services_->find(service_path);
            if (iter != sub_services_->end()) {
                iter->second.properties_["enable"] = value;
                iter->second.enabled_ = (value == "1");
            } else {
                log_err("service %s not subscribed, why we get this event???",
                        service_path);
                code = -1;
            }
        } else {
            log_err("retrieve value for path %s failed!", service_path);
            code = -1;
        }
        return code;
    } else if (type == ZOO_CHILD_EVENT) {
        // recevied when add/remove new properties or node
        return internal_subscribe_service(department, service);
    } else if (type == ZOO_SESSION_EVENT) {
        // Painic
        log_err("should not handle session_event in zkFrame here!");
        return -1;
    } else if (type == ZOO_NOTWATCHING_EVENT) {
        // rewatch this service
        return internal_subscribe_service(department, service);
    }

    log_err("unhandled event %s, path %s", zkClient::zevent_str(type), service_path);
    return -1;
}

int zkFrame::internal_handle_zk_service_properties_event(int type, const char* service_property_path) {

    std::string department;
    std::string service;
    std::string property;
    if (!ServiceType::service_property_parse(service_property_path, department, service, property)) {
        log_err("invalid service property path: %s", service_property_path);
        return -1;
    }

    if (type == ZOO_CREATED_EVENT) {
        // Panic
        log_err("should not receive event %s for %s",
                zkClient::zevent_str(type), service_property_path);
        return -1;
    } else if (type == ZOO_DELETED_EVENT) {
        // 服务目录内容的增加删除会得到 ZOO_CHILD_EVENT，在那边自动处理
        return 0;
    } else if (type == ZOO_CHANGED_EVENT) {
        // 普通的服务节点属性更新
        std::string value;
        std::string service_path = zkPath::make_path(department, service);
        int code = 0;
        if (client_->zk_get(service_property_path, value, 1, NULL) == 0) {
            std::lock_guard<std::mutex> lock(lock_);
            auto iter = sub_services_->find(service_path);
            if (iter != sub_services_->end()) {
                iter->second.properties_[property] = value;
            } else {
                log_err("service %s not subscribed, why we get this event???",
                        service_path.c_str());
                code = -1;
            }
        } else {
            log_err("retrieve value for path %s failed.", service_property_path);
            code = -1;
        }
        return code;
    } else if (type == ZOO_CHILD_EVENT) {
        // property should not have child path
        log_err("service_property should not have child path, and we should not watch it acitvely: %s",
                service_property_path);
        return -1;
    } else if (type == ZOO_SESSION_EVENT) {
        // Painic
        log_err("should not handle session_event in zkFrame here!");
        return -1;
    } else if (type == ZOO_NOTWATCHING_EVENT) {
        // rewatch this service
        return internal_subscribe_service(department, service);
    }

    log_err("unhandled event %s, path %s", zkClient::zevent_str(type), service_property_path);
    return -1;
}

int zkFrame::internal_handle_zk_node_event(int type, const char* node_path) {

    std::string department;
    std::string service;
    std::string node;
    if (!NodeType::node_parse(node_path, department, service, node)) {
        log_err("invalid node path: %s", node_path);
        return -1;
    }

    if (type == ZOO_CREATED_EVENT) {
        // Panic
        log_err("should not receive event %s for %s",
                zkClient::zevent_str(type), node_path);
        return -1;
    } else if (type == ZOO_DELETED_EVENT) {
        // service will recv ZOO_CHILD_EVENT and handle it
        return 0;
    } else if (type == ZOO_CHANGED_EVENT) {
        // 节点启用禁用
        std::string value;
        std::string service_path = zkPath::make_path(department, service);
        int code = 0;
        if (client_->zk_get(node_path, value, 1, NULL) == 0) {
            std::lock_guard<std::mutex> lock(lock_);
            auto iter = sub_services_->find(service_path);
            if (iter != sub_services_->end()) {
                auto node_p = iter->second.nodes_.find(node);
                if (node_p != iter->second.nodes_.end()) {
                    node_p->second.properties_["enable"] = value;
                    node_p->second.enabled_ = (value == "1");
                } else {
                    log_err("node %s not found in sub_service, why we get this event?",
                            node.c_str());
                    code = -1;
                }
            } else {
                log_err("service %s not subscribed, why we get this event???", service_path.c_str());
                log_err("full nodes info for %s info:\n %s",
                        service_path.c_str(), iter->second.str().c_str());
                code = -1;
            }
        } else {
            log_err("retrieve value for path %s failed.", node_path);
            code = -1;
        }
        return code;
    } else if (type == ZOO_CHILD_EVENT) {
        // When add/remove new properties for node
        return internal_subscribe_node(node_path);
    } else if (type == ZOO_SESSION_EVENT) {
        // Painic
        log_err("should not handle session_event in zkFrame here!");
        return -1;
    } else if (type == ZOO_NOTWATCHING_EVENT) {
        // rewatch this node
        return internal_subscribe_node(node_path);
    }

    log_err("unhandled event %s, path %s", zkClient::zevent_str(type), node_path);
    return -1;
}

int zkFrame::internal_handle_zk_node_properties_event(int type, const char* node_property_path) {

    std::string department;
    std::string service;
    std::string node;
    std::string property;
    if (!NodeType::node_property_parse(node_property_path,
                                       department, service, node, property)) {
        log_err("invalid node property path: %s", node_property_path);
        return -1;
    }

    if (type == ZOO_CREATED_EVENT) {
        // Panic
        log_err("should not receive event %s for %s",
                zkClient::zevent_str(type), node_property_path);
        return -1;
    } else if (type == ZOO_DELETED_EVENT) {
        // 节点目录会得到 ZOO_CHILD_EVENT，在那边处理
        return 0;
    } else if (type == ZOO_CHANGED_EVENT) {
        std::string value;
        std::string service_path = zkPath::make_path(department, service);
        int code = 0;
        if (client_->zk_get(node_property_path, value, 1, NULL) == 0) {
            std::lock_guard<std::mutex> lock(lock_);
            auto iter = sub_services_->find(service_path);
            if (iter != sub_services_->end()) {
                auto node_p = iter->second.nodes_.find(node);
                if (node_p != iter->second.nodes_.end()) {
                    node_p->second.properties_[property] = value;
                } else {
                    log_err("node %s not found in sub_service, why we get this event?",
                            node.c_str());
                    log_err("full nodes info for %s info:\n %s",
                            service_path.c_str(), iter->second.str().c_str());
                    code = -1;
                }
            } else {
                log_err("service %s not subscribed, why we get this event???", service_path.c_str());
                code = -1;
            }
        } else {
            log_err("retrieve value for path %s failed.", node_property_path);
            code = -1;
        }
        return code;
    } else if (type == ZOO_CHILD_EVENT) {
        // property should not have child path
        log_err("node_property should not have child path, and we should not watch it acitvely: %s",
                node_property_path);
        return -1;
    } else if (type == ZOO_SESSION_EVENT) {
        // Painic
        log_err("should not handle session_event in zkFrame here!");
        return -1;
    } else if (type == ZOO_NOTWATCHING_EVENT) {
        // rewatch this service
        return internal_subscribe_node(zkPath::make_path(department, service, node).c_str());
    }

    log_err("unhandled event %s, path %s", zkClient::zevent_str(type), node_property_path);
    return -1;
}


// internal
// 根据0.0.0.0扩充得到实体节点
int zkFrame::substitute_node(const NodeType& node, std::vector<NodeType>& nodes) {

    if (node.department_.empty() || node.service_.empty() || !zkPath::validate_node(node.node_)) {
        log_err("invalid Node parameter provide.");
        return -1;
    }

    std::string host = node.node_.substr(0, node.node_.find(":"));
    std::string port = node.node_.substr(node.node_.find(":") + 1);
    nodes.clear();

    // 提供实体节点
    if (host != "0.0.0.0") {
        NodeType n_node = node;
        n_node.idc_ = idc_;
        n_node.host_ = host;
        n_node.port_ = ::atoi(port.c_str());
        nodes.push_back(n_node);
        return 0;
    }

    // 添加本地实际的物理地址
    for (size_t i = 0; i < whole_nodes_addr_.size(); ++i) {
        NodeType n_node = node;
        n_node.node_ = whole_nodes_addr_[i] + ":" + port;
        n_node.idc_ = idc_;
        n_node.host_ = whole_nodes_addr_[i];
        n_node.port_ = ::atoi(port.c_str());
        nodes.push_back(n_node);
    }

    return 0;
}




} // Clotho

