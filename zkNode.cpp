#include "zkPath.h"
#include "zkNode.h"

namespace Clotho {

std::string NodeType::str() const {
    std::stringstream ss;

    ss << "node info => "
        << "fullpath: " << department_ <<", "<< service_ <<", "<< node_ << std::endl
        << "host&port: " << host_ <<", " << port_ << std::endl
        << "active: " << (active_ ? "on" : "off") << std::endl
        << "enabled: " << (enabled_ ? "on" : "off") << std::endl
        << "idc: " << idc_ << std::endl
        << "priority: " << priority_ << std::endl
        << "weight: " << weight_ << std::endl;

    ss << "priorities: " << std::endl;
    for (auto iter=properties_.begin(); iter!=properties_.end(); ++iter) {
        ss << "\t" << iter->first << " - " << iter->second << std::endl;
    }

    return ss.str();
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
            idc_= iter->second;
            continue;
        }

        if (iter->first == "weight") {
            int weight = ::atoi(iter->second.c_str());
            if (weight >= 0 && weight <= std::numeric_limits<uint8_t>::max())
                weight_ = weight;
            continue;
        }

        if (iter->first == "priority") {
            int priority = ::atoi(iter->second.c_str());
            if (priority >= 0 && priority <= std::numeric_limits<uint8_t>::max())
                priority_ = priority;
            continue;
        }

        paths.emplace_back(std::pair<std::string, std::string>
                           (zkPath::extend_property(node_path, iter->first), iter->second));
    }

    // add 3 default
    paths.emplace_back(std::pair<std::string, std::string>
                       (zkPath::extend_property(node_path, "idc"), idc_));

    paths.emplace_back(std::pair<std::string, std::string>
                       (zkPath::extend_property(node_path, "weight"),   to_string(weight_)));
    paths.emplace_back(std::pair<std::string, std::string>
                       (zkPath::extend_property(node_path, "priority"), to_string(priority_)));

    return true;
}


// ServiceType
std::string ServiceType::str() const {
    std::stringstream ss;

    ss << "service info => "
     << "fullpath: " << department_ <<", "<< service_ << std::endl
     << "active: " << (active_ ? "on" : "off") << std::endl
     << "enabled: " << (enabled_ ? "on" : "off") << std::endl
     << "pick_strategy: " << pick_strategy_ << std::endl
     << "nodes count: " << nodes_.size() << std::endl;

    ss << "priorities: " << std::endl;
    for (auto iter=properties_.begin(); iter!=properties_.end(); ++iter) {
        ss << "\t" << iter->first << " - " << iter->second << std::endl;
    }

    return ss.str();
}



} // Clotho

