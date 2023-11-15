/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __HLS_COMMON_H
#define __HLS_COMMON_H

#include "compat.h"
#include <string>
#include <list>

#ifndef _WIN32
#include <sys/time.h>
#else
#include <winsock2.h>
#endif /* _WIN32 */

#ifndef NO_SSL
#include "ssl.h"
#endif

namespace hls
{
    struct chunk_info
    {
        long id;

        unsigned long usec;

        std::string url;

#ifndef NO_SSL
        std::string key_method;
        std::string key_url;
        std::string key_iv;
#endif
        chunk_info():id(0),usec(0) {}
    };

    class chunks_list
    {
    protected:
        std::list<chunk_info> chunks;

        int chunks_num;

        long last_chunk_id;

        unsigned long target_duration;
    public:
        chunks_list(void):chunks_num(0),last_chunk_id(-1),target_duration(0) {}

#ifndef NO_SSL
        void push_back(long id,unsigned long usec,std::string& url, std::string& key_method, std::string& key_url, std::string& key_iv)
#else
        void push_back(long id,unsigned long usec,std::string& url)
#endif
        {
            chunks.push_back(chunk_info());

            chunk_info& c=chunks.back();

            c.id=id; c.usec=usec; c.url.swap(url);

#ifndef NO_SSL
            c.key_method.swap(key_method); c.key_url.swap(key_url); c.key_iv.swap(key_iv);
#endif

            last_chunk_id=c.id;

            chunks_num++;
        }

        void pop_front(chunk_info& c)
        {
            chunk_info& cc=chunks.front();

            c.id=cc.id; c.usec=cc.usec; c.url.swap(cc.url);

#ifndef NO_SSL
            c.key_method.swap(cc.key_method); c.key_url.swap(cc.key_url); c.key_iv.swap(cc.key_iv);
#endif

            chunks.pop_front();

            chunks_num--;
        }

        bool empty(void) { return chunks_num>0?false:true; }

        int size(void) { return chunks_num; }

        long get_last_chunk_id(void) { return last_chunk_id; }

        void set_target_duration(unsigned long n) { target_duration=n; }

        unsigned long get_target_duration(void) { return target_duration; }

        void resize(int max_num)
        {
            if(chunks_num>max_num)
            {
                for(int i=max_num;i<chunks_num;i++)
                    chunks.pop_front();

                chunks_num=max_num;
            }
        }
    };

    class vbuf
    {
    protected:
        socket_t fd;

#ifndef NO_SSL
        SSL_CTX* ssl_ctx;

        SSL* ssl;
#endif

        char buffer[512];

        int offset;

        int size;

        unsigned long total_read;
    public:
        vbuf(void):fd(-1),offset(0),size(0),total_read(0)
#ifndef NO_SSL
        ,ssl_ctx(nullptr),ssl(nullptr)
#endif
        {}

        int gets(char* p,int len);

        int read(char* p,int len);

        int write(const char* p,int len);

        void close(void);
    };

    class stream : public vbuf
    {
    protected:
        std::string user_agent;

        long long int len;

        bool is_len;

        std::string content_range;

        std::string base_url;

        bool connect(const std::string& host,int port);

        bool _open(const std::string& url,const std::string& range,const std::string& post_data,std::string& location);
    public:
        stream(const std::string& _user_agent=std::string()):user_agent(_user_agent),len(0),is_len(false) {}

        ~stream(void) { close(); }

        bool open(const std::string& url,const std::string& range=std::string(),const std::string& post_data=std::string());

        int parse_stream_info(int stream_id,chunks_list& chunks);

        long long int length(void) { return len; }

        bool is_length_present(void) { return is_len; }

        const std::string& range(void) { return content_range; }

        void swap(stream& s)
            { int _fd=s.fd; s.fd=fd; fd=_fd; long long int _len=s.len; s.len=len; len=_len; }

        void clear(void) { len=0; is_len=false; content_range.clear(); base_url.clear(); }
    };

#ifdef _WIN32
    struct timeval
    {
        unsigned long tv_sec;
        unsigned long tv_usec;
    };

    void usleep(long usec);

    int gettimeofday(timeval* tv,void*);
#endif

    void tv_now(timeval& tv);

    unsigned long elapsed_usec(timeval& tv0);

    void split_url(const std::string& url,std::string& stream_url,std::string& user_agent,int* stream_id);
}

namespace http
{
    void sendurl(const std::string& url);

    int fetch(const std::string& url,std::string& dst,const std::string& post_data=std::string());
}

namespace utils
{
    std::string extract_root_url(const std::string& url);
}

#endif
