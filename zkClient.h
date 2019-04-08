#ifndef __ZK_CLIENT_H__
#define __ZK_CLIENT_H__

#include <string>
#include <vector>

#include <memory>
#include <mutex>

// replace with Log.h latter
#include <cstdio>
#define log_debug(fmt, ...) ::printf("DEBUG [%s:%d(%s)]" fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define log_info(fmt, ...)  ::printf("TRACE [%s:%d(%s)]" fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define log_err(fmt, ...)   ::printf("ERROR [%s:%d(%s)]" fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

// 使用ZooKeeper客户端库和ZooKeeper Server通信的封装

// type forward
struct _zhandle;
struct Stat;
struct ACL_vector;

namespace tzkeeper {

class zkClient {

public:
    zkClient(const std::string& hostline, const std::string& idc, int session_timeout = 10 * 1000):
        hostline_(hostline),
        idc_(idc),
        session_timeout_(session_timeout),
        zhandle_lock_(),
        zhandle_(NULL) {
    }

    ~zkClient() {
    }


    int zk_init();

    int zk_create(const char* path, const std::string& value, const struct ACL_vector *acl, int flags);
    int zk_delete(const char* path, int version = -1);

    int zk_set(const char* path, const std::string& value, int version = -1);
    int zk_get(const char* path, std::string& value, int watch, struct Stat* stat);

    int zk_exists(const char* path, int watch, struct Stat *stat);
    int zk_get_children(const char* path, int watch, std::vector<std::string>& children);


private:

    // conf
    std::string               hostline_;
    std::vector<std::string>  hosts_;
    std::string               idc_;
    int                       session_timeout_;

    // internal handle and sync
    std::mutex                zhandle_lock_;
    struct _zhandle*          zhandle_;
};

} // end namespace tzkeeper

#endif // __ZK_CLIENT_H__

