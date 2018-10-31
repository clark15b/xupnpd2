/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "plugin_tsbuf.h"
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

namespace tsbuf
{
    class buf_t
    {
    protected:
        char* buffer;

        int max_len,spos,rpos,len;
    public:
        buf_t(int _len):max_len(_len),spos(0),rpos(0),len(0) { buffer=(char*)malloc(_len); }

        ~buf_t(void) { free(buffer); }

        int size(void) { return len; }

        bool empty(void) { return len>0?false:true; }

        int get_write_ptr(char** ptr)
        {
            if(spos==max_len)
                spos=0;

            int n=max_len-spos; int m=max_len-len;

            n=n>m?m:n;

            *ptr=buffer+spos;

            return n;
        }

        void seek_write_ptr(int n)
            { len+=n; spos+=n; }

        int get_read_ptr(char** ptr)
        {
            if(rpos==max_len)
                rpos=0;

            int n=max_len-rpos;

            if(len<n)
                n=len;

            *ptr=buffer+rpos;

            return n;
        }

        void seek_read_ptr(int n)
            { len-=n; rpos+=n; }
    };

    int max_buf_size=0;

    int min_buf_size=0;

    static const int ts_frame_size=188;

    inline int round_to_frame_size(int size)
        { return ( (size/ts_frame_size) + (size%ts_frame_size?1:0) ) * ts_frame_size; }
}

void tsbuf::sendurl(const std::string&)
{
    const char* opts=getenv("OPTS");

    if(opts && *opts)
    {
        char* endptr=NULL;

        max_buf_size=(int)strtol(opts,&endptr,10);

        if(*endptr=='k' || *endptr=='K')
            max_buf_size*=1024;
        else if(*endptr=='m' || *endptr=='M')
            max_buf_size*=1024*1024;

        const char* p=strpbrk(opts,";,");

        if(p)
        {
            min_buf_size=(int)strtol(p+1,&endptr,10);

            if(*endptr=='k' || *endptr=='K')
                min_buf_size*=1024;
            else if(*endptr=='m' || *endptr=='M')
                min_buf_size*=1024*1024;
        }
    }

    const unsigned int def_max=512*1024;

    if(max_buf_size<def_max)
        max_buf_size=def_max;

    if(min_buf_size<1 || min_buf_size>max_buf_size)
        min_buf_size=max_buf_size;

    max_buf_size=round_to_frame_size(max_buf_size);

    min_buf_size=round_to_frame_size(min_buf_size);

    buf_t buffer(max_buf_size);

    fcntl(0,F_SETFL,fcntl(0,F_GETFL)|O_NONBLOCK);

    fcntl(1,F_SETFL,fcntl(1,F_GETFL)|O_NONBLOCK);

    char __buf[ts_frame_size*1000];                             // max drop size

    bool ready=false;

    bool done=false;

    for(;;)
    {
        fd_set rfdset; FD_ZERO(&rfdset); fd_set* prfdset=NULL;

        fd_set wfdset; FD_ZERO(&wfdset); fd_set* pwfdset=NULL;

        int maxn=0;

        if(!done)
            { FD_SET(0,&rfdset); maxn++; prfdset=&rfdset; }

        if(ready && !buffer.empty())
            { FD_SET(1,&wfdset); maxn++; pwfdset=&wfdset; }

        if(!maxn)
            break;

        int rc=select(maxn,prfdset,pwfdset,NULL,NULL);

        if(rc==-1)
            break;

        if(FD_ISSET(1,&wfdset))
        {
            bool is_eof=false;

            for(;;)
            {
                char* ptr; int size=buffer.get_read_ptr(&ptr);

                if(size<1)
                    break;

                ssize_t n=write(1,ptr,size);

                if(n>0)
                    buffer.seek_read_ptr(n);
                else
                {
                    if(!n || (n==(ssize_t)-1 && errno!=EAGAIN))
                        { is_eof=true; break; }
                }

            }

            if(is_eof)
                break;
        }

        if(FD_ISSET(0,&rfdset))
        {
            char* ptr; int size=buffer.get_write_ptr(&ptr);

            ssize_t n;

            if(size>0)
            {
                n=read(0,ptr,size);

                if(n && n!=(ssize_t)-1)
                {
                    buffer.seek_write_ptr(n);

                    if(buffer.size()>=min_buf_size)
                        ready=true;
                }
            }else
                { n=read(0,__buf,sizeof(__buf)); }              // drop if recv speed > send speed

            if(n==0 || n==(ssize_t)-1)
                ready=done=true;
        }
    }
}
