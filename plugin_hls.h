/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __HLS_H
#define __HLS_H

#include "plugin_hls_common.h"

namespace hls
{
    bool update_stream_info(const std::string& _url,int stream_id,const std::string& user_agent,chunks_list& chunks,class context& ctx);

    class context
    {
    protected:
        std::string callback;

        int refresh_period;

        time_t last_update_time;

        std::string current_url;

        std::string method;

    public:
        context(void);

        bool resolv_url(const std::string& url,std::string& real_url);
    };

    void sendurl(const std::string& url);

    void sendurl2(const std::string& url);

    void sendurl3(const std::string& url);
}

#endif
