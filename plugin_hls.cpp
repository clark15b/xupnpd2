/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "plugin_hls.h"
#include "plugin_lua.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#include <time.h>
#endif /* _WIN32 */

hls::context::context(void)
{
    refresh_period=0;

    last_update_time=0;

    const char* opts=getenv("OPTS");

    if(!opts)
        return;

    const char* p=strpbrk(opts,",;");

    if(p)
        { callback.assign(opts,p-opts); refresh_period=atoi(p+1); }
    else
        callback.assign(opts);

    if(refresh_period<15)
        refresh_period=15;
}

bool hls::context::resolv_url(const std::string& url,std::string& real_url)
{
    if(!callback.empty())
    {
        time_t now=time(NULL);

        if(!last_update_time || now-last_update_time>refresh_period)
        {
            current_url=luas::translate_url(callback,url);

            last_update_time=now;
        }

        real_url=current_url;

        if(real_url.empty())
            return false;
    }

    return true;
}

bool hls::update_stream_info(const std::string& _url,int stream_id,const std::string& user_agent,chunks_list& chunks,context& ctx)
{
    std::string url(_url);

    // call hook for URL translation
    if(!ctx.resolv_url(_url,url))
        return false;

    stream s(user_agent);

    if(!s.open(url))
        return false;

    int num=s.parse_stream_info(stream_id,chunks);

    s.close();

    if(num>0 && chunks.size()>num*2)
        chunks.resize(num/2);

    return true;
}

// bit rate control
void hls::sendurl2(const std::string& url)
{
    context ctx;

    std::string stream_url;

    std::string user_agent;

    int stream_id=0;

    split_url(url,stream_url,user_agent,&stream_id);

    chunks_list chunks;

    bool quit=false;

    do
    {
        timeval start_tv; tv_now(start_tv);

        // get stream info

        if(chunks.size()<2)
        {
            if(!update_stream_info(stream_url,stream_id,user_agent,chunks,ctx))
                break;
        }

        if(chunks.empty())
            break;

        chunk_info c;

        chunks.pop_front(c);

        // connect

        stream s(user_agent);

        if(s.open(c.url))
        {
            int chunk_sec=c.usec/1000000;

            {
                int t=elapsed_usec(start_tv)/1000000+1;

                if(t<c.usec)
                    chunk_sec-=t;
            }

            int bps=s.length()/chunk_sec;

            if(s.length()%chunk_sec)
                bps++;

            // sending chunk

            char buf[4096];

            long bytes_left_total=(long)s.length();

            long bytes_left=bytes_left_total>bps?bps:bytes_left_total;

            timeval tv; tv_now(tv);

            while(bytes_left_total>0)
            {
                // read

                int n=s.read(buf,bytes_left>sizeof(buf)?sizeof(buf):bytes_left);

                if(n<1)
                    break;

                bytes_left_total-=n;

                bytes_left-=n;

                // write

                int bytes_sent=0;

                while(bytes_sent<n)
                {
                    int m=::write(1,buf+bytes_sent,n-bytes_sent);

                    if(m<1)
                        { quit=true; break; }

                    bytes_sent+=m;
                }

                if(quit)
                    break;

                // bit rate control

                if(bytes_left<1)
                {
                    unsigned long t=elapsed_usec(tv);

                    if(t<1000000)
                        usleep(1000000-t);

                    bytes_left=bytes_left_total>bps?bps:bytes_left_total;

                    tv_now(tv);
                }
            }

            // delay

            if(c.usec>0)
            {
                unsigned long t=elapsed_usec(start_tv);

                if(t<c.usec)
                    usleep(c.usec-t);
            }

            s.close();
        }
    }while(!quit && !chunks.empty());
}

// fast start, bit rate control
void hls::sendurl3(const std::string& url)
{
    context ctx;

    std::string stream_url;

    std::string user_agent;

    int stream_id=0;

    split_url(url,stream_url,user_agent,&stream_id);

    chunks_list chunks;

    bool quit=false;

    while(!quit)
    {
        timeval start_tv; tv_now(start_tv);

        // get stream info

        if(chunks.size()<2)
        {
            if(!update_stream_info(stream_url,stream_id,user_agent,chunks,ctx))
                break;
        }

        if(chunks.empty())
        {
            unsigned long delay=chunks.get_target_duration()/2;

            if(delay<1000000)
                delay=1000000;

            usleep(delay);

            continue;
        }

        chunk_info c;

        chunks.pop_front(c);

        // connect

        stream s(user_agent);

        if(s.open(c.url))
        {
            int chunk_sec=(c.usec-elapsed_usec(start_tv))/1000000;

            int bps=s.length()/chunk_sec;

            if(s.length()%chunk_sec)
                bps++;

            // sending chunk

            char buf[4096];

            long bytes_left_total=(long)s.length();

            long bytes_left=bytes_left_total>bps?bps:bytes_left_total;

            timeval tv; tv_now(tv);

            while(bytes_left_total>0)
            {
                // read

                int n=s.read(buf,bytes_left>sizeof(buf)?sizeof(buf):bytes_left);

                if(n<1)
                    break;

                bytes_left_total-=n;

                bytes_left-=n;

                // write

                int bytes_sent=0;

                while(bytes_sent<n)
                {
                    int m=::write(1,buf+bytes_sent,n-bytes_sent);

                    if(m<1)
                        { quit=true; break; }

                    bytes_sent+=m;
                }

                if(quit)
                    break;

                // bit rate control

                if(bytes_left<1)
                {
                    unsigned long t=elapsed_usec(tv);

                    if(chunks.empty() && t<1000000)
                        usleep(1000000-t);

                    bytes_left=bytes_left_total>bps?bps:bytes_left_total;

                    tv_now(tv);
                }
            }

            // delay

            if(c.usec>0 && chunks.empty())
            {
                unsigned long t=elapsed_usec(start_tv);

                if(t<c.usec)
                    usleep(c.usec-t);
            }

            s.close();
        }
    }
}

// as is
void hls::sendurl(const std::string& url)
{
    context ctx;

    std::string stream_url;

    std::string user_agent;

    int stream_id=0;

    split_url(url,stream_url,user_agent,&stream_id);

    chunks_list chunks;

    bool quit=false;

    timeval last_update_tv; tv_now(last_update_tv);

    while(!quit)
    {
        // update playlist

        unsigned long target_duration=chunks.get_target_duration();

        unsigned long elapsed=elapsed_usec(last_update_tv);

        if(elapsed>=target_duration)
        {
            tv_now(last_update_tv);

            if(!update_stream_info(stream_url,stream_id,user_agent,chunks,ctx))
                break;
        }

        while(chunks.empty())
        {
            unsigned long delay=chunks.get_target_duration()/2;

            if(delay<1000000)
                delay=1000000;

            usleep(delay);

            tv_now(last_update_tv);

            if(!update_stream_info(stream_url,stream_id,user_agent,chunks,ctx))
                break;
        }

        if(chunks.empty() || !chunks.get_target_duration())
            break;

        // get stream info

        chunk_info c;

        chunks.pop_front(c);

        // connect

        stream s(user_agent);

        if(s.open(c.url))
        {
            // sending chunk

            char buf[4096];

            long bytes_left=(long)s.length();

            if(bytes_left<1)
                bytes_left=999999999;

            while(bytes_left>0)
            {
                // read

                int n=s.read(buf,bytes_left>sizeof(buf)?sizeof(buf):bytes_left);

                if(n<1)
                    break;

                bytes_left-=n;

                // write

                int bytes_sent=0;

                while(bytes_sent<n)
                {
                    int m=::write(1,buf+bytes_sent,n-bytes_sent);

                    if(m<1)
                        { quit=true; break; }

                    bytes_sent+=m;
                }

                if(quit)
                    break;
            }

            s.close();
        }
    }
}
