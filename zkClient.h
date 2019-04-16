#ifndef __CLOTHO_CLIENT_H__
#define __CLOTHO_CLIENT_H__

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
struct zoo_op;
struct zoo_op_result;

struct Stat;
struct ACL_vector;

namespace Clotho {

typedef std::function<int(int,int,const char*)> BizEventFunc;

class zkClient {

public:
    zkClient(const std::string& hostline, const BizEventFunc& func = BizEventFunc(), 
             const std::string& idc = "default", int session_timeout = 10*1000);
    ~zkClient();

    // 该函数是可重复调用的，当会话断开的时候使用这个来重建会话
    bool zk_init();
    int handle_session_event(int type, int state, const char *path);
    int delegete_biz_event(int type, int state, const char *path);

    int zk_create(const char* path, const std::string& value, const struct ACL_vector *acl, int flags);
    int zk_delete(const char* path, int version = -1);

    int zk_set(const char* path, const std::string& value, int version = -1);
    int zk_get(const char* path, std::string& value, int watch, struct Stat* stat);

    // 1 存在，0不存在，其他请求失败
    int zk_exists(const char* path, int watch, struct Stat *stat);
    int zk_get_children(const char* path, int watch, std::vector<std::string>& children);

    int zk_multi(int op_count, const struct zoo_op *ops, struct zoo_op_result *results);

private:

    // conf
    std::string               hostline_;
    std::vector<std::string>  hosts_;
    std::string               idc_;
    int                       session_timeout_;
    
    std::function<int(int,int,const char*)> biz_event_func_;

    // internal handle and sync
    std::mutex                zhandle_lock_;
    struct _zhandle*          zhandle_;
};

} // end namespace Clotho

#endif // __CLOTHO_CLIENT_H__

