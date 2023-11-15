/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "live.h"

#include "common.h"
#include "db.h"

#include <list>

#ifndef _WIN32
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#else
#include <winsock2.h>
#endif /* _WIN32 */

#include "plugin_hls.h"
#include "plugin_hls_new.h"
#include "plugin_hls_decrypt.h"
#include "plugin_tsbuf.h"
#include "plugin_lua.h"
#include "plugin_udprtp.h"
#include "plugin_tsfilter.h"

namespace live
{
    int split_handlers(const std::string& s,std::list<handler_desc_t>& handlers);

    typedef void (*handler_proc_t)(const std::string& url);

    std::map<std::string,handler_proc_t> builtin;

#ifndef _WIN32
    void terminate_handlers(std::list<pid_t>& handlers)
    {
        for(std::list<pid_t>::const_iterator it=handlers.begin();it!=handlers.end();++it)
            { kill(*it,SIGTERM); waitpid(*it,NULL,0); }
    }

    socket_t start_handler(std::list<pid_t>& handlers,const std::string& handler,const std::string& url,const std::map<std::string,std::string>& env,socket_t input_fd);
#else
    struct thread_ctx_t
    {
        socket_t output_fd;
        HANDLE input_fd;
    };

    DWORD WINAPI __thread_proc(LPVOID p)
    {
        socket_t output_fd=((thread_ctx_t*)p)->output_fd;

        HANDLE input_fd=((thread_ctx_t*)p)->input_fd;

        char buf[1024];

        for(;;)
        {
            int n=recv(output_fd,buf,sizeof(buf),0);

            if(n==0 || n==SOCKET_ERROR)
                break;
        }

        CancelIoEx(input_fd,NULL);

        ExitThread(0);
    }

    void terminate_handlers(std::list<HANDLE>& handlers)
    {
        for(std::list<HANDLE>::const_iterator it=handlers.begin();it!=handlers.end();++it)
            { TerminateProcess(*it,0); WaitForSingleObject(*it,500); CloseHandle(*it); }
    }

    HANDLE start_handler(std::list<HANDLE>& handlers,const std::string& handler,const std::string& url,const std::map<std::string,std::string>& env,HANDLE input_fd);
#endif /* _WIN32 */
}

bool live::init(void)
{
    builtin["http"]=http::sendurl;
    builtin["udp"]=udprtp::sendurl;
    builtin["rtp"]=udprtp::sendurl;
    builtin["tsfilter"]=tsfilter::sendurl;

    builtin["hls"]=hls::sendurl;
    builtin["hls2"]=hls::sendurl2;
    builtin["hls3"]=hls::sendurl3;
    builtin["hls4"]=hls_new::sendurl;
    builtin["lua"]=luas::sendurl;

#ifndef _WIN32
    builtin["tsbuf"]=tsbuf::sendurl;
#endif /* _WIN32 */

#ifndef NO_SSL
    builtin["hlse"] = hls_decrypt::sendurl;
#endif

    return true;
}

void live::done(void) {}


int live::req::__get_extra_headers(std::string& dst)
{
    int retval=0;

    for(std::string::size_type p1=0,p2;p1!=std::string::npos;p1=p2)
    {
        std::string s;

        p2=headers.find_first_of("\r\n",p1);

        if(p2!=std::string::npos)
        {
            s=headers.substr(p1,p2-p1);

            if(headers[++p2]=='\n')
                ++p2;
        }else
            s=headers.substr(p1);

        if(!s.empty())
        {
            if(!strncasecmp(s.c_str(),"Status:",7))
                retval=atoi(utils::trim(s.substr(7)).c_str());
            else if(!strncasecmp(s.c_str(),"Content-Range:",14) || !strncasecmp(s.c_str(),"Content-Length:",15) ||
                        !strncasecmp(s.c_str(),"Accept-Ranges:",14))
                            { dst.append(s); dst.append("\r\n"); }
        }
    }

    return retval;
}

bool live::req::__send_headers(socket_t sock,bool is_extra)
{
    is_headers_sent=true;

    int status=0; std::string extras2;

    if(is_extra)
        status=__get_extra_headers(extras2);

    client->headers(status?status:200,true,(u_int64_t)-1,mime);

    for(std::map<std::string,std::string>::const_iterator it=extras->begin();it!=extras->end();++it)
        client->out->printf("%s: %s\r\n",it->first.c_str(),it->second.c_str());

    if(!extras2.empty())
        client->out->printf("%s",extras2.c_str());

    client->out->printf("\r\n");

    if(client->method=="HEAD")
        return false;

    client->out->flush();

    // direct access to sockets now!

    return true;
}

bool live::req::__send(socket_t sock,const char* buf,int len)
{
    if(!is_headers_sent)
    {
        bool __stop=false;

        for(;!__stop && len>0;buf++,len--)
        {
            int ch=*((const unsigned char*)buf);

            switch(st)
            {
            case 0:
                headers+=ch;

                if(headers.length()>6)
                {
                    if(!strcasecmp(headers.c_str(),"Status:"))
                        st=1;
                    else
                        { st=20; __stop=true; }
                }
                break;
            case 1:
                headers+=ch;

                if(ch=='\r')
                    st=2;
                else if(ch=='\n')
                    st=3;
                break;
            case 2:
                headers+=ch;

                if(ch=='\n')
                    st=3;
                else
                    st=1;
                break;
            case 3:
                headers+=ch;

                if(ch=='\r')
                    st=4;
                else if(ch=='\n')
                    { st=10; __stop=true; }
                else
                    st=1;
                break;
            case 4:
                headers+=ch;

                if(ch=='\n')
                    { st=10; __stop=true; }
                else
                    st=1;
                break;
            }
        }

        if(st==10)
        {
            if(!__send_headers(sock,true))
                return false;

            headers.clear();
        }else if(st==20 || headers.length()>max_headers_len)
        {
            if(!__send_headers(sock,false))
                return false;

            if(!__real_send(sock,headers.c_str(),headers.length()))
                return false;

            headers.clear();
        }else
            return true;
    }

    return __real_send(sock,buf,len);
}

bool live::req::__real_send(socket_t sock,const char* buf,int len)
{
    int l=0;

    while(!http::quit && l<len)
    {
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sock,&fdset);

        timeval tv; tv.tv_sec=cfg::live_snd_timeout; tv.tv_usec=0;

        int rc=select(sock+1,NULL,&fdset,NULL,&tv);

        if(!rc)             // skip data from live stream
            return true;

        if(rc!=1)
            return false;

#ifndef _WIN32
        int n=write(sock,buf+l,len-l);
#else
        int n=send(sock,buf+l,len-l,0);
#endif /* _WIN32 */

        if(n<1)
            return false;

        l+=n;
    }

    return true;
}

#ifndef _WIN32
bool live::req::__forward(socket_t src,socket_t dst)
{
    char buf[2048];

    int n=read(src,buf,sizeof(buf));

    if(n<1)
        return false;

    if(!__send(dst,buf,n))
        return false;

    return true;
}

#else
bool live::req::__forward(void* src,socket_t dst)
{
    char buf[2048];

    DWORD n=0;

    if(!ReadFile((HANDLE)src,buf,sizeof(buf),&n,NULL) || n<1)
        return false;

    if(!__send(dst,buf,n))
        return false;

    return true;
}

#endif /* _WIN32 */

int live::split_handlers(const std::string& s,std::list<handler_desc_t>& handlers)
{
    int num=0;

    for(std::string::size_type p1=0,p2;p1!=std::string::npos;p1=p2)
    {
        std::string handler,opts;

        p2=s.find('/',p1);

        if(p2!=std::string::npos)
            { handler=s.substr(p1,p2-p1); p2++; }
        else
            handler=s.substr(p1);

        std::string::size_type n=handler.find_first_of("[(");

        if(n!=std::string::npos)
        {
            int ch=(handler[n]=='(')?')':']';

            std::string::size_type n2=handler.find(ch,n);

            if(n2!=std::string::npos)
                { opts=handler.substr(n+1,n2-n-1); handler=handler.substr(0,n); }
        }

        n=handler.find('.');

        if(n!=std::string::npos && handler.substr(n+1)=="lua")
            { opts.swap(handler); handler="lua"; }

        if(!handler.empty())
            { handlers.push_back(handler_desc_t(handler,opts)); num++; }
    }

    return num;
}

bool live::sendurl(http::req* req,const std::string& url,const std::string& handler,const char* mime,std::map<std::string,std::string>& extras)
{
// hls,http,udp,file
    req->is_keep_alive=false;

    live::req _req(req,mime,&extras);

#ifndef _WIN32
    std::list<pid_t> handlers_chain;
#else
    std::list<HANDLE> handlers_chain;
#endif /* _WIN32 */

    std::list<handler_desc_t> handlers;

    split_handlers(handler,handlers);

    std::string real_url(url);

    std::string url_translator;

    if(!handlers.empty())
    {
        handler_desc_t& h=handlers.front();

        if(h.handler[0]=='@')
            { url_translator=h.handler.substr(1); handlers.pop_front(); }
    }

    if(!url_translator.empty())
    {
        real_url=luas::translate_url(url_translator,url);

        if(real_url.empty())
            return false;
    }

    socket_t output_fd=req->out->fileno();

#ifndef _WIN32
    socket_t input_fd=open("/dev/null",O_RDONLY);
#else
    HANDLE input_fd=INVALID_HANDLE_VALUE;
#endif /* _WIN32 */

    std::map<std::string,std::string> env;
    env["URL"]=real_url;
    env["CONTENT_TYPE"]=mime;

#ifdef _WIN32
    {
        const char* p=getenv("SystemRoot");
        if(p)
            env["SystemRoot"]=p;
    }
#endif /* _WIN32 */

    {
        std::map<std::string,std::string>::const_iterator it=req->hdrs.find("range");

        if(it!=req->hdrs.end())
            env["RANGE"]=it->second;
    }

    for(std::list<handler_desc_t>::const_iterator it=handlers.begin();it!=handlers.end();++it)
    {
        const handler_desc_t& h=*it;

        env["HANDLER"]=h.handler;
        env["OPTS"]=h.opts;

        input_fd=start_handler(handlers_chain,h.handler,real_url,env,input_fd);
    }

#ifndef _WIN32
    if(input_fd!=-1)
    {
        while(!http::quit)
        {
            fd_set fdset; FD_ZERO(&fdset);

            FD_SET(input_fd,&fdset);
            FD_SET(output_fd,&fdset);

            int maxn=input_fd<output_fd?output_fd:input_fd;

            timeval tv; tv.tv_sec=cfg::live_rcv_timeout; tv.tv_usec=0;

            int rc=select(maxn+1,&fdset,NULL,NULL,&tv);

            if(rc!=1)
                break;

            if(FD_ISSET(output_fd,&fdset))
            {
                char buf[1024];

                if(read(output_fd,buf,sizeof(buf))<1)
                    break;
            }

            if(FD_ISSET(input_fd,&fdset) && !_req.__forward(input_fd,output_fd))
                break;
        }

        close(input_fd);
    }
#else
    if(input_fd!=INVALID_HANDLE_VALUE)
    {
        thread_ctx_t ctx;
        ctx.output_fd=output_fd;
        ctx.input_fd=input_fd;

        HANDLE thr=CreateThread(NULL,0,__thread_proc,(void*)&ctx,0,NULL);

        if(thr)
        {
            // ignore cfg::live_rcv_timeout :(
            while(!http::quit && _req.__forward((void*)input_fd,output_fd));

            closesocket(output_fd);

            if(WaitForSingleObject(thr,1000)!=WAIT_OBJECT_0)
                TerminateThread(thr,0);

            CloseHandle(thr);
        }

        CloseHandle(input_fd);
    }
#endif /* _WIN32 */

    if(!_req.is_headers_sent)                   // if no content from handlers
        req->headers(200,false,0,mime);

    terminate_handlers(handlers_chain);

    return false;
}

#ifndef _WIN32
socket_t live::start_handler(std::list<pid_t>& handlers,const std::string& handler,const std::string& url,const std::map<std::string,std::string>& env,socket_t input_fd)
{
    utils::trace(utils::log_core,"using handler '%s' for '%s'",handler.c_str(),url.c_str());

    int __output[2]={-1,-1};

    if(pipe(__output))
        { close(input_fd); return -1; }

    pid_t pid=fork();

    if(pid==(pid_t)-1)
        { close(input_fd); close(__output[0]); close(__output[1]); return -1; }

    if(!pid)
    {
        dup2(input_fd,0);

        dup2(__output[1],1);

        dup2(open("/dev/null",O_WRONLY),2);

        signal(SIGHUP,SIG_DFL);
        signal(SIGPIPE,SIG_DFL);
        signal(SIGUSR1,SIG_DFL);
        signal(SIGUSR2,SIG_DFL);
        signal(SIGINT,SIG_DFL);
        signal(SIGQUIT,SIG_DFL);
        signal(SIGALRM,SIG_DFL);
        signal(SIGTERM,SIG_DFL);
        signal(SIGCHLD,SIG_DFL);

        sigset_t sigset;
        sigfillset(&sigset);
        sigprocmask(SIG_UNBLOCK,&sigset,0);

        for(std::map<std::string,std::string>::const_iterator it=env.begin();it!=env.end();++it)
            setenv(it->first.c_str(),it->second.c_str(),1);

        std::map<std::string,handler_proc_t>::iterator it=builtin.find(handler);

        if(it!=builtin.end())
            it->second(url);
        else
        {
            char path[512];

            snprintf(path,sizeof(path),"%s",handler.c_str());

            path[sizeof(path)-1]=0;

            execlp(path,path,(const char*)NULL);
        }

        exit(123);
    }

    handlers.push_back(pid);

    close(input_fd);

    close(__output[1]);

    return __output[0];
}
#else
HANDLE live::start_handler(std::list<HANDLE>& handlers,const std::string& handler,const std::string& url,const std::map<std::string,std::string>& env,HANDLE input_fd)
{
    utils::trace(utils::log_core,"using handler '%s' for '%s'",handler.c_str(),url.c_str());

    HANDLE __output[2]={INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE};

    SECURITY_ATTRIBUTES attr;
    ZeroMemory(&attr,sizeof(attr));
    attr.nLength=sizeof(attr);
    attr.bInheritHandle=TRUE;
    attr.lpSecurityDescriptor=NULL;

    if(!CreatePipe(&__output[0],&__output[1],&attr,0))
        { CloseHandle(input_fd); return INVALID_HANDLE_VALUE; }

    SetHandleInformation(__output[0],HANDLE_FLAG_INHERIT,0);

    char path[MAX_PATH]="";

    std::map<std::string,handler_proc_t>::iterator it=builtin.find(handler);

    if(it!=builtin.end())
    {
        int n=GetModuleFileName(NULL,path,sizeof(path));

        if(n>0)
            snprintf(path+n,sizeof(path)-n," %s",handler.c_str());
    }

    if(!*path)
        snprintf(path,sizeof(path),"%s.exe",handler.c_str());

    path[sizeof(path)-1]=0;

    STARTUPINFO si;
    ZeroMemory(&si,sizeof(si));
    si.cb=sizeof(si);
    si.dwFlags|=STARTF_USESTDHANDLES;
    si.hStdInput=input_fd;
    si.hStdOutput=__output[1];
    si.hStdError=INVALID_HANDLE_VALUE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi,sizeof(pi));

    std::string _env;

    for(std::map<std::string,std::string>::const_iterator it=env.begin();it!=env.end();++it)
        { _env.append(it->first); _env+='='; _env.append(it->second.c_str(),it->second.length()+1); }

    _env+='\0';

    if(!CreateProcess(NULL,path,NULL,NULL,TRUE,CREATE_NO_WINDOW,(void*)_env.c_str(),NULL,&si,&pi))
        { CloseHandle(input_fd); CloseHandle(__output[0]); CloseHandle(__output[1]); return INVALID_HANDLE_VALUE; }

    handlers.push_back(pi.hProcess);

    CloseHandle(pi.hThread);

    CloseHandle(input_fd);

    CloseHandle(__output[1]);

    return __output[0];
}
#endif /* _WIN32 */
