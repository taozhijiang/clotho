/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#ifndef __CLOTHO_CONSTRUCT_EXCEPTION_H__
#define __CLOTHO_CONSTRUCT_EXCEPTION_H__

#include <exception>
#include <cstring>


namespace Clotho {
    
class ConstructException: public std::exception {
    
public:
    ConstructException(const char* msg) {
        ::memset(msg_, 0, kMaxExceptionMsgSize);
        std::strcpy(msg_, "ConstructException: ");
        if(msg)
            std::strncpy(msg_ + strlen("ConstructException: "), msg, 
                         kMaxExceptionMsgSize - strlen("ConstructException: ") - 1 );
    }
    
    ConstructException(const ConstructException& e) {
        ::memcpy(msg_, e.msg_, kMaxExceptionMsgSize);
    }
    
    virtual ~ConstructException() throw () /*noexcept*/ {
    }
    
    const char* what() const throw () /*noexcept*/ /*override*/ { 
        return msg_; 
    }
 
protected:
    static const size_t kMaxExceptionMsgSize = 1024;
    char msg_[kMaxExceptionMsgSize];
};


} // end namespace Clotho

#endif //__CLOTHO_CONSTRUCT_EXCEPTION_H__ss
