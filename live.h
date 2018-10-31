/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __LIVE_H
#define __LIVE_H

#include <string>
#include "http.h"

namespace live
{
    struct handler_desc_t
    {
        std::string handler;
        std::string opts;

        handler_desc_t(const std::string& _handler,const std::string& _opts):handler(_handler),opts(_opts) {}
    };

    class req
    {
    protected:
        http::req* client;

        const char* mime;

        std::map<std::string,std::string>* extras;

        int st;

        enum { max_headers_len=512 };

        std::string headers;

        int __get_extra_headers(std::string& dst);

        bool __send_headers(socket_t sock,bool is_extra);

        bool __real_send(socket_t sock,const char* buf,int len);

        bool __send(socket_t sock,const char* buf,int len);

    public:
        bool is_headers_sent;

        req(http::req* _client,const char* _mime,std::map<std::string,std::string>* _extras):client(_client),mime(_mime),extras(_extras),
            st(0),is_headers_sent(false) {}

#ifndef _WIN32
        bool __forward(socket_t src,socket_t dst);
#else
        bool __forward(void* src,socket_t dst);
#endif /* _WIN32 */

    };

    bool init(void);

    bool sendurl(http::req* req,const std::string& url,const std::string& handler,const char* mime,std::map<std::string,std::string>& extras);

    void done(void);
}

#endif
