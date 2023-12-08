/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "plugin_hls_new.h"
#include "plugin_lua.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#else
#include <windows.h>
#include <io.h>
#include <time.h>
#include <list>
#endif /* _WIN32 */

namespace hls_new
{
    using namespace hls;

    class metastream
    {
    protected:
        std::string master_url;

        std::string user_agent;

        int stream_id;

        std::string callback;

        int refresh_period;

        time_t last_update_time;

        std::string current_url;

        std::string method;

#ifndef _WIN32
        FILE* fp;

        int pfd[2];

        pid_t pid;
#else
        HANDLE thrid;

        DWORD tid;

        std::list<std::string> pipe;

        CRITICAL_SECTION cs;

        HANDLE sem;

        static DWORD WINAPI __doit(LPVOID lpParameter)
            { ((metastream*)lpParameter)->doit(0); ExitThread(0); }
#endif /* _WIN32 */

        void doit(int fd);
    public:
        metastream(void):stream_id(0),refresh_period(0),last_update_time(0),
#ifndef _WIN32
            fp(NULL),pid((pid_t)-1)
                { pfd[0]=pfd[1]=-1; }

        ~metastream(void) {}
#else
            thrid(NULL),tid(0)
                { InitializeCriticalSection(&cs); sem=CreateSemaphore(NULL,0,1000,NULL); }

        ~metastream(void)
        {
            if(thrid!=NULL)
            {
                TerminateThread(thrid,0);

                CloseHandle(thrid);
            }

            CloseHandle(sem);

            DeleteCriticalSection(&cs);
        }
#endif /* _WIN32 */

        bool init(const std::string& url);

        const std::string& get_user_agent(void) { return user_agent; }

        bool update_stream_info(chunks_list& chunks);

        bool get_next_chunk_url(std::string& url);
    };
}

bool hls_new::metastream::init(const std::string& url)
{
    split_url(url,master_url,user_agent,&stream_id);

    const char* opts=getenv("OPTS");

    method=getenv("METHOD");

    if(opts)
    {
        const char* p=strpbrk(opts,",;");

        if(p)
            { callback.assign(opts,p-opts); refresh_period=atoi(p+1); }
        else
            callback.assign(opts);

        if(refresh_period<15)
            refresh_period=15;
    }

#ifndef _WIN32
    if(pipe(pfd))
        return false;

    pid=fork();

    if(pid==(pid_t)-1)
        { close(pfd[0]); close(pfd[1]); return false; }
    else if(!pid)
        { close(pfd[0]); doit(pfd[1]); close(pfd[1]); exit(0); }

    close(pfd[1]);

    fp=fdopen(pfd[0],"r");
#else
    thrid=CreateThread(NULL,0,__doit,this,0,&tid);

    if(thrid==NULL)
        return false;
#endif /* _WIN32 */

    return true;
}

bool hls_new::metastream::update_stream_info(hls::chunks_list& chunks)
{
    std::string url(master_url);

    // call hook for URL translation

    if(!callback.empty())
    {
        time_t now=time(NULL);

        if(!last_update_time || now-last_update_time>refresh_period)
        {
            current_url=luas::translate_url(callback,url,method);

            last_update_time=now;
        }

        url=current_url;

        if(url.empty())
            return false;
    }

    stream s(user_agent);

    if(!s.open(url))
        return false;

    int num=s.parse_stream_info(stream_id,chunks);

    s.close();

    if(num>0 && chunks.size()>num*2)
        chunks.resize(num/2);

    return true;
}

void hls_new::metastream::doit(int fd)
{
#ifndef _WIN32
    fp=fdopen(pfd[1],"w");

    if(!fp)
        return;
#endif /* _WIN32 */

    chunks_list chunks;

    for(;;)
    {
        // update playlist

        unsigned long delay=0;

#ifndef _WIN32
        timeval last_update_tv;
#else
        hls::timeval last_update_tv;
#endif /* _WIN32 */

        tv_now(last_update_tv);

        update_stream_info(chunks);

        if(!chunks.empty())
        {
            // get stream info

            chunk_info c;

#ifndef _WIN32
            while(!chunks.empty())
            {
                chunks.pop_front(c);

                fprintf(fp,"%s\n",c.url.c_str());
            }

            fflush(fp);
#else

            int pushed=0;

            EnterCriticalSection(&cs);

            while(!chunks.empty())
            {
                chunks.pop_front(c);

                pipe.push_front(c.url);

                pushed++;
            }

            LeaveCriticalSection(&cs);

            if(pushed>0)
                ReleaseSemaphore(sem,pushed,NULL);

#endif /* _WIN32 */

            unsigned long elapsed=elapsed_usec(last_update_tv);

            if(elapsed<c.usec)
                delay=c.usec-elapsed;
        }else
        {
            if(chunks.get_target_duration()>0)
                delay=chunks.get_target_duration()/2;
            else
                delay=1000000;
        }

        if(delay>0)
            usleep(delay);
    }
#ifndef _WIN32
    fclose(fp);
#else
    ReleaseSemaphore(sem,1,NULL);
#endif /* _WIN32 */
}

#ifndef _WIN32
bool hls_new::metastream::get_next_chunk_url(std::string& url)
{
    if(!fp)
        return false;

    char buf[1024];

    if(!fgets(buf,sizeof(buf),fp))
        return false;

    char* p=strpbrk(buf,"\r\n");
    if(!p)
        return false;

    url.assign(buf,p-buf);

    if(url.empty())
        return false;

    return true;
}
#else
bool hls_new::metastream::get_next_chunk_url(std::string& url)
{
    if(thrid==NULL || WaitForSingleObject(sem,INFINITE)!=WAIT_OBJECT_0)
        return false;

    bool rc=false;

    EnterCriticalSection(&cs);

    if(!pipe.empty())
    {
        pipe.back().swap(url);
        pipe.pop_back();
        rc=true;
    }

    LeaveCriticalSection(&cs);

    return rc;
}
#endif /* _WIN32 */

void hls_new::sendurl(const std::string& url)
{
    metastream meta;

    if(!meta.init(url))
        return;

    std::string chunk_url;

    bool quit=false;

    while(meta.get_next_chunk_url(chunk_url))
    {
        stream s(meta.get_user_agent());

        if(s.open(chunk_url))
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
