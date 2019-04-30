#include <string>

#include <memory>
#include <iostream>

#include "zkFrame.h"

using namespace Clotho;

int main(int argc, char* argv[]) {
    
    if(argc < 2) {
        std::cout << "clotho_lock master [20]" << std::endl;
        return -1;
    }
    
    std::string lock_name = std::string(argv[1]);
    uint32_t sec = 0;
    if(argc >= 3)
        sec = ::atoi(argv[2]);
    
    zkFrame* client_ = new zkFrame("aliyun");
    if(!client_->init("127.0.0.1:2181,127.0.0.1:2182"))
        return -1;
    
    if(argc >= 3) {
        if(!client_->recipe_service_try_lock("dept", "srv_inst", "master", sec))
            return 1;
    } else {
        if(!client_->recipe_service_lock("dept", "srv_inst", "master"))
            return 1;
    }
    
    if(client_->recipe_service_lock_owner("dept", "srv_inst", "master"))
        std::cout << "request lock success!" << std::endl;
    else 
        std::cout << "request lock failed!" << std::endl;

    ::sleep(3);
    
    client_->revoke_all_nodes();
    delete client_;
        
    return 0;
}
