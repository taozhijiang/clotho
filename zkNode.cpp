#include <ostream>

#include "zkPath.h"
#include "zkFrame.h"
#include "zkNode.h"

namespace Clotho {

NodeType::NodeType(const std::string& department, const std::string& service, const std::string& node,
                   const std::map<std::string, std::string>& properties) :
    department_(department), service_(service), node_(node),
    host_(), port_(0),
    active_(false), enabled_(true),
    idc_(),
    priority_(kWPDefault),
    weight_(kWPDefault),
    properties_(properties) {
}

std::string NodeType::str() const {
    std::stringstream ss;

    ss  << std::endl
        << "node info => "
        << "fullpath: " << department_ << ", " << service_ << ", " << node_ << std::endl
        << "host & port: " << host_ << ", " << port_ << std::endl
        << "active: " << (active_ ? "on" : "off") << std::endl
        << "enabled: " << (enabled_ ? "on" : "off") << std::endl
        << "idc: " << idc_ << std::endl
        << "priority: " << priority_ << std::endl
        << "weight: " << weight_ << std::endl;

    ss << "properties: " << std::endl;
    for (auto iter = properties_.begin(); iter != properties_.end(); ++iter) {
        ss << "\t" << iter->first << " - " << iter->second << std::endl;
    }

    return ss.str();
}


std::ostream& operator<<(std::ostream& os, const NodeType& node) {
    os << node.str() << std::endl;
    return os;
}

// NodeType
bool NodeType::prepare_path(VectorPair& paths) {

    if (department_.empty() || service_.empty() || !zkPath::validate_node(node_)) {
        log_err("invalid Node parameter provide.");
        return false;
    }

    paths.clear();

    paths.emplace_back(std::pair<std::string, std::string>("/" + department_, "1"));
    paths.emplace_back(std::pair<std::string, std::string>("/" + department_ + "/" + service_, "1"));

    std::string node_path = zkPath::make_path(department_, service_, node_);
    paths.emplace_back(std::pair<std::string, std::string>(node_path, "1"));

    for (auto iter = properties_.begin(); iter != properties_.end(); ++iter) {

        if (iter->first == "idc") {
            idc_ = iter->second;
            continue;
        }

        if (iter->first == "weight") {
            int weight = ::atoi(iter->second.c_str());
            if (weight >= kWPMin && weight <= kWPMax)
                weight_ = weight;
            continue;
        }

        if (iter->first == "priority") {
            int priority = ::atoi(iter->second.c_str());
            if (priority >= kWPMin && priority <= kWPMax)
                priority_ = priority;
            continue;
        }

        // 临时的保留节点名
        if(iter->first == "active") {
            log_err("active is reserved, should not put in properties.");
            continue;
        }

        paths.emplace_back(std::pair<std::string, std::string>
                           (zkPath::extend_property(node_path, iter->first), iter->second));
    }

    // add 3 default
    paths.emplace_back(std::pair<std::string, std::string>
                       (zkPath::extend_property(node_path, "idc"), idc_));

    paths.emplace_back(std::pair<std::string, std::string>
                       (zkPath::extend_property(node_path, "weight"),   Clotho::to_string(weight_)));
    paths.emplace_back(std::pair<std::string, std::string>
                       (zkPath::extend_property(node_path, "priority"), Clotho::to_string(priority_)));

    return true;
}

bool NodeType::node_parse(const char* fp, std::string& d, std::string& s, std::string& n) {

    auto pt = zkPath::guess_path_type(fp);
    if (pt != PathType::kNode)
        return false;

    std::vector<std::string> vec;
    zkPath::split(fp, "/", vec);
    if (vec.size() != 3) return false;
    if (!zkPath::validate_node(vec[2])) return false;

    d = vec[0];
    s = vec[1];
    n = vec[2];
    return true;
}

bool NodeType::node_property_parse(const char* fp,
                                   std::string& d, std::string& s, std::string& n, std::string& p) {

    auto pt = zkPath::guess_path_type(fp);
    if (pt != PathType::kNodeProperty)
        return false;

    std::vector<std::string> vec;
    zkPath::split(fp, "/", vec);
    if (vec.size() != 4) return false;
    if (!zkPath::validate_node(vec[2])) return false;

    d = vec[0];
    s = vec[1];
    n = vec[2];
    p = vec[3];
    return true;
}


// ServiceType

ServiceType::ServiceType(const std::string& department, const std::string& service,
                         const std::map<std::string, std::string>& properties) :
    department_(department), service_(service),
    enabled_(true),
    pick_strategy_(kStrategyDefault),
    nodes_(),
    properties_(properties) {
}

std::string ServiceType::str() const {
    std::stringstream ss;

    ss << std::endl
        << "service info => "
        << "fullpath: " << department_ << ", " << service_ << std::endl
        << "enabled: " << (enabled_ ? "on" : "off") << std::endl
        << "pick_strategy: " << pick_strategy_ << std::endl
        << "nodes count: " << nodes_.size() << std::endl;

    ss << "properities: " << std::endl;
    for (auto iter = properties_.begin(); iter != properties_.end(); ++iter) {
        ss << "\t" << iter->first << " - " << iter->second << std::endl;
    }

    ss << "full node list:" << std::endl;
    for (auto iter = nodes_.begin(); iter != nodes_.end(); ++iter) {
        ss << "\t ~" << iter->first.c_str() << std::endl;
        ss << "\t" << iter->second.str().c_str() << std::endl;
    }

    return ss.str();
}

std::ostream& operator<<(std::ostream& os, const ServiceType& srv) {
    os << srv.str() << std::endl;
    return os;
}

bool ServiceType::service_parse(const char* fp, std::string& d, std::string& s) {

    auto pt = zkPath::guess_path_type(fp);
    if (pt != PathType::kService)
        return false;

    std::vector<std::string> vec;
    zkPath::split(fp, "/", vec);
    if (vec.size() != 2) return false;

    d = vec[0];
    s = vec[1];
    return true;
}


bool ServiceType::service_property_parse(const char* fp,
                                         std::string& d, std::string& s, std::string& p) {

    auto pt = zkPath::guess_path_type(fp);
    if (pt != PathType::kServiceProperty)
        return false;

    std::vector<std::string> vec;
    zkPath::split(fp, "/", vec);
    if (vec.size() != 3) return false;

    d = vec[0];
    s = vec[1];
    p = vec[2];
    return true;
}


} // Clotho

