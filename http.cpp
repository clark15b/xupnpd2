/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "http.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "soap.h"
#include "mime.h"
#include "db.h"
#include "ssdp.h"
#include "scripting.h"
#include "live.h"

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#else
#include <io.h>
#include <winsock2.h>
#endif /* _WIN32 */

namespace http
{
    volatile bool quit=false;

    socket_t incoming_socket=INVALID_SOCKET;

    struct err_t { int code; const char* msg; };

    struct range_t { u_int64_t from; u_int64_t to; bool is_valid; };

    err_t errors[]=
    {
        { 100, "Continue" },
        { 101, "Switching Protocols" },
        { 200, "OK" },
        { 201, "Created" },
        { 202, "Accepted" },
        { 203, "Non-Authoritative Information" },
        { 204, "No Content" },
        { 205, "Reset Content" },
        { 206, "Partial Content" },
        { 300, "Multiple Choices" },
        { 301, "Moved Permanently" },
        { 302, "Moved Temporarily" },
        { 303, "See Other" },
        { 304, "Not Modified" },
        { 305, "Use Proxy" },
        { 400, "Bad Request" },
        { 401, "Unauthorized" },
        { 402, "Payment Required" },
        { 403, "Forbidden" },
        { 404, "Not Found" },
        { 405, "Method Not Allowed" },
        { 406, "Not Acceptable" },
        { 407, "Proxy Authentication Required" },
        { 408, "Request Time-Out" },
        { 409, "Conflict" },
        { 410, "Gone" },
        { 411, "Length Required" },
        { 412, "Precondition Failed" },
        { 413, "Request Entity Too Large" },
        { 414, "Request-URL Too Large" },
        { 415, "Unsupported Media Type" },
        { 416, "Requested range not satisfiable" },
        { 500, "Internal Server error" },
        { 501, "Not Implemented" },
        { 502, "Bad Gateway" },
        { 503, "Out of Resources" },
        { 504, "Gateway Time-Out" },
        { 505, "HTTP Version not supported" },
        { 0,   NULL }
    };

    std::map<int,const char*> http_err;

    bool get_range(std::map<std::string,std::string>& hdrs,range_t& range,struct stat64& st);

    bool mk_incoming_socket(void);

    socket_t get_incoming_socket(void) { return incoming_socket; }

    void done(void)
    {
        if(incoming_socket!=INVALID_SOCKET)
            { shutdown(incoming_socket,2); closesocket(incoming_socket); incoming_socket=INVALID_SOCKET; }
    }
}

int http::obuffer::append(const char* p,int len)
{
    int n=max_buf_size-size;

    if(n<=0)
        return 0;

    if(n>len)
        n=len;

    memcpy(buf+size,p,n);

    size+=n;

    return n;
}

http::stream::~stream(void)
    { flush(); shutdown(fd,2); closesocket(fd); }

void http::stream::alarm(int n)
    { __alarm_time=n>0?time(0)+n:0; }

ssize_t http::stream::os_send(socket_t sockfd,const char* buf,size_t len)
{
    if(quit)
        return -1;

    if(cfg::http_snd_timeout>0)
    {
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sockfd,&fdset);

        timeval tv; tv.tv_sec=cfg::http_snd_timeout; tv.tv_usec=0;

        if(select(sockfd+1,NULL,&fdset,NULL,&tv)!=1)
            return -1;
    }
#ifndef _WIN32
    return ::write(sockfd,buf,len);
#else
    int n=::send(sockfd,buf,len,0); return n==SOCKET_ERROR?-1:n;
#endif /* _WIN32 */
}

ssize_t http::stream::os_recv(socket_t sockfd,char* buf,size_t len)
{
    if(quit)
        return -1;

    if(__alarm_time>0)
    {
        time_t now=time(0);

        if(now>=__alarm_time)
            return -1;

        fd_set fdset; FD_ZERO(&fdset); FD_SET(sockfd,&fdset);

        timeval tv; tv.tv_sec=__alarm_time-now; tv.tv_usec=0;

        if(select(sockfd+1,&fdset,NULL,NULL,&tv)!=1)
            return -1;
    }
#ifndef _WIN32
    return ::read(sockfd,buf,len);
#else
    int n=::recv(sockfd,buf,len,0); return n==SOCKET_ERROR?-1:n;
#endif /* _WIN32 */
}

bool http::stream::__write(const char* p,int len)
{
    int l=0;

    while(l<len)
    {
        ssize_t n=os_send(fd,p+l,len-l);

        if(n==(ssize_t)-1 || n==0)
            break;
        l+=n;
    }
    return l==len?true:false;
}

bool http::stream::printf(const char* fmt,...)
{
    char buf[512];

    va_list ap;

    va_start(ap,fmt);

    int n=vsnprintf(buf,sizeof(buf),fmt,ap);

    va_end(ap);

    if(n==-1 || n>=sizeof(buf))
        return false;

    return write(buf,n);
}

bool http::stream::flush(void)
{
    if(obuf.size>0)
    {
        if(!__write(obuf.buf,obuf.size))
            return false;

        obuf.reset();
    }

    return true;
}

bool http::stream::write(const char* p,int len)
{
    while(len>0)
    {
        int n=obuf.append(p,len);

        if(n>0)
            { p+=n; len-=n; }

        if(obuf.size>=max_buf_size)
        {
            if(!__write(obuf.buf,max_buf_size))
                return false;

            obuf.reset();
        }

        if(len>=max_buf_size)
            return __write(p,len);
    }

    return true;
}

int http::stream::__getc(void)
{
    if(ibuf.empty())
    {
        ssize_t n=os_recv(fd,ibuf.buf,max_buf_size);

        if(n==(ssize_t)-1 || n==0)
            { is_eof=true; return EOF; }

        ibuf.size=n; ibuf.offset=0;
    }

    return ((unsigned char*)ibuf.buf)[ibuf.offset++];
}


int http::stream::gets(char* s,int max)
{
    if(is_eof)
        return EOF;

    int len=0;

    while(len<max-1)
    {
        int ch=__getc();

        if(ch==EOF)
        {
            if(len==0)
                return EOF;
            else
                break;
        }

        if(ch=='\n')
            break;
        else if(ch=='\r')
            continue;
        else
            ((unsigned char*)s)[len++]=(unsigned char)ch;
    }

    s[len]=0;

    return len;
}

int http::stream::read(char* s,int max)
{
    if(is_eof)
        return EOF;

    int n=ibuf.size-ibuf.offset;

    if(n>0)
    {
        if(n>max)
            n=max;

        memcpy(s,ibuf.buf+ibuf.offset,n);

        ibuf.offset+=n;

        return n;
    }

    ssize_t nn=os_recv(fd,s,max);

    if(nn==(ssize_t)-1 || nn==0)
        return EOF;

    return (int)nn;
}

bool http::mk_incoming_socket(void)
{
    socket_t s=socket(AF_INET,SOCK_STREAM,0);

    if(s==INVALID_SOCKET)
        return false;

    int reuse=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));

    sockaddr_in sin;
    sin.sin_family=AF_INET;
    sin.sin_addr.s_addr=ssdp::get_lan_if_addr();
    sin.sin_port=htons(cfg::http_port);
#if defined(__FreeBSD__) || defined(__APPLE__)
    sin.sin_len=sizeof(sin);
#endif /* __FreeBSD__ */

#ifndef _WIN32
    if(bind(s,(sockaddr*)&sin,sizeof(sin)) || listen(s,cfg::http_backlog) || fcntl(s,F_SETFL,fcntl(s,F_GETFL)|O_NONBLOCK))
#else
    u_long nonb=1;
    if(bind(s,(sockaddr*)&sin,sizeof(sin)) || listen(s,cfg::http_backlog) || ioctlsocket(s,FIONBIO,&nonb))
#endif /* _WIN32 */
        { closesocket(s); return false; }

    incoming_socket=s;

    return true;
}

void http::process_query(sockaddr_in& sin,stream& fp)
{
    int keep_alive_count=0;

    bool is_keep_alive=true;

    while(!quit && is_keep_alive)
    {
        keep_alive_count++;

        fp.alarm(cfg::http_keep_alive_timeout);

        int idx=0;

        http::req req;

        req.client_ip=utils::inet_ntoa(sin.sin_addr);

        bool is_post=false;

        char buf[1024];

        int buf_len;

        while(!quit && (buf_len=fp.gets(buf,sizeof(buf)))!=EOF)
        {
            fp.alarm(cfg::http_rcv_timeout);

            char* p=buf+buf_len;

            while(p>buf && p[-1]==' ')

                p--;
            *p=0;

            if(!*buf)
            {
                if(idx>0)
                    break;
                else
                    continue;
            }

            if(idx==0)
            {
                p=strchr(buf,' ');

                if(!p)
                    return;

                if(!strncasecmp(buf,"POST",p-buf))
                    is_post=true;

                req.method.assign(buf,p-buf);

                while(*p==' ') p++;

                char* p2=strstr(p," HTTP/1.");

                if(!p2)
                    return;

                is_keep_alive=(p2[8]=='0')?false:true;

                while(p2>p && p2[-1]==' ') p2--;

                if(p2<=p)
                    return;

                req.url.assign(p,p2-p);
            }else
            {
                char* p2,*p3;

                for(p=buf;*p==' ';++p);

                for(p2=p;*p2 && *p2!=':';++p2)
                    *p2=tolower(*p2);

                if(*p2!=':')
                    return;

                for(p3=p2+1;*p3==' ';p3++);

                while(p2>p && p2[-1]==' ') p2--;

                req.hdrs[std::string(p,p2-p)]=p3;
            }

            ++idx;
        }

        if(buf_len==EOF || req.url.empty())
            return;

        {
            std::map<std::string,std::string>::const_iterator it=req.hdrs.find("connection");

            if(it!=req.hdrs.end())
            {
                if(!strcasecmp(it->second.c_str(),"Keep-Alive"))
                    is_keep_alive=true;
                else
                    is_keep_alive=false;
            }
        }

        if(is_post)
        {
            std::map<std::string,std::string>::const_iterator it=req.hdrs.find("content-length");

            if(it!=req.hdrs.end() && !it->second.empty())
            {
                int size=atoi(it->second.c_str());

                if(size<0 || size>cfg::http_max_post_size)
                    return;

                if(size>0)
                {
                    req.data.reserve(size);

                    int n;

                    while(!quit && size>0 && (n=fp.read(buf,size>sizeof(buf)?sizeof(buf):size))!=EOF)
                        { req.data.append(buf,n); size-=n; }

                    if(size>0)
                        return;
                }
            }
        }

        fp.alarm(0);

        if(keep_alive_count>=cfg::http_keep_alive_max)
            is_keep_alive=false;

        req.is_keep_alive=is_keep_alive;
        req.out=&fp;

        {
            std::string::size_type n=req.url.find('?');

            if(n!=std::string::npos)
            {
                std::string args=req.url.substr(n+1);

                req.url=req.url.substr(0,n);

                http::split_http_args(args,req.args);
            }
        }

        bool rc=req.main();

        req.out->flush();

        if(!rc)
            break;

        is_keep_alive=req.is_keep_alive;
    }
}

void http::split_http_args(const std::string& s,std::map<std::string,std::string>& args)
{
    for(std::string::size_type p1=0,p2;p1!=std::string::npos;p1=p2)
    {
        p2=s.find('&',p1);

        std::string ss;

        if(p2!=std::string::npos)
            { ss=s.substr(p1,p2-p1); p2++; }
        else
            ss=s.substr(p1);

        std::string::size_type p3=ss.find('=');

        if(p3!=std::string::npos)
            args[ss.substr(0,p3)]=ss.substr(p3+1);
    }
}

bool http::init(void)
{
    for(int i=0;errors[i].code>0;i++)
        http_err[errors[i].code]=errors[i].msg;

    if(!mk_incoming_socket())
        { utils::trace(utils::log_err,"unable to create tcp socket for http exchange, location: %s",cfg::www_location.c_str()); return false; }

    return true;
}

std::string http::content_type_by_name(const std::string& filename)
{
    std::string::size_type n=filename.find_last_of('.');

    if(n!=std::string::npos)
    {
        mime::type_t* t=mime::get_by_name(filename.substr(n+1));

        if(t)
            return t->mime;
    }

    return "application/x-octet-stream";
}

bool http::get_range(std::map<std::string,std::string>& hdrs,range_t& range,struct stat64& st)
{
    range.is_valid=false;

    if(!st.st_size)
        return false;

    std::map<std::string,std::string>::const_iterator it=hdrs.find("range");

    if(it==hdrs.end())
        return false;

    const std::string& _range=it->second;

    if(strncasecmp(_range.c_str(),"bytes=",6))
        return false;

    const char* p=_range.c_str()+6;
    const char* p2=strchr(p,'-');

    if(!p2)
        return false;

    std::string from(p,p2-p),to(p2+1);

    if(!from.empty())
    {
        long long int f=strtoll(from.c_str(),NULL,10);

        if(f<0 || f>=st.st_size)
            return false;

        range.from=f;

        if(to.empty())
            range.to=st.st_size-1;
        else
        {
            long long int t=strtoll(to.c_str(),NULL,10);

            if(t<range.from)
                return false;

            if(t>=st.st_size)
                range.to=st.st_size-1;
            else
                range.to=t;
        }
    }else
    {
        if(to.empty())
            return false;

        long long int t=strtoll(to.c_str(),NULL,10);

        if(t<1)
            return false;

        if(t>st.st_size)
            { range.from=0; range.to=st.st_size-1; }
        else
            { range.from=st.st_size-t; range.to=st.st_size-1; }
    }

    range.is_valid=true;

    return true;
}

bool http::req::headers(int code,bool extra,u_int64_t content_length,const std::string& content_type)
{
    const char* status="";

    {
        std::map<int,const char*>::const_iterator it=http_err.find(code);

        if(it==http_err.end())
            http_err.find(500);

        status=it->second;
    }

    out->printf(
        "HTTP/1.1 %d %s\r\n"
        "Server: %s\r\n"
        "Date: %s\r\n"
        "Connection: %s\r\n"
        "EXT:\r\n",
        code,status,cfg::ssdp_server.c_str(),utils::sys_time(0).c_str(),is_keep_alive?"Keep-Alive":"close");

    if(is_keep_alive)
        out->printf("Keep-Alive: timeout=%u, max=%d\r\n",cfg::http_keep_alive_timeout,cfg::http_keep_alive_max);

    if(content_length!=(u_int64_t)-1)
        out->printf("Content-Length: %llu\r\n",(unsigned long long int)content_length);

    if(!content_type.empty())
        out->printf("Content-Type: %s\r\n",content_type.c_str());

    if(!extra)
        out->printf("\r\n");


    if(utils::is_trace_needed(utils::log_http))
    {
        std::string s=utils::format("%s \"%s %s\"",client_ip.c_str(),method.c_str(),url.c_str());

        if(utils::is_trace_needed(utils::log_http_hdrs))
        {
            for(std::map<std::string,std::string>::const_iterator it=hdrs.begin();it!=hdrs.end();++it)
                s+=utils::format(" %s='%s'",it->first.c_str(),it->second.c_str());
        }

        if(content_length==(u_int64_t)-1)
            s+=utils::format(" %d - \"%s\"",code,status);
        else
            s+=utils::format(" %d %llu \"%s\"",code,(unsigned long long int)content_length,status);

        utils::trace(utils::log_http,"%s",s.c_str());
    }

    return true;
}

bool http::req::sendfile(const std::string& filename,const std::map<std::string,std::string>& extras)
{
    int fd=open(filename.c_str(),O_RDONLY|O_LARGEFILE|O_BINARY);

    if(fd==-1)
        return headers(404,false);

    struct stat64 st;

    if(!fstat64(fd,&st))
    {
        u_int64_t flen=st.st_size;

        range_t range;

        if(get_range(hdrs,range,st))
        {
            st.st_size=range.to-range.from+1;

            if(lseek64(fd,(off64_t)range.from,SEEK_SET)==(off64_t)-1)
                { close(fd); return false;}
        }

        headers(range.is_valid?206:200,true,st.st_size,content_type_by_name(filename));

        out->printf("Last-Modified: %s\r\nAccept-Ranges: bytes\r\n",utils::sys_time(st.st_mtime).c_str());

        if(range.is_valid)
            out->printf("Content-Range: bytes %llu-%llu/%llu\r\n",
                (unsigned long long int)range.from,(unsigned long long int)range.to,(unsigned long long int)flen);

        for(std::map<std::string,std::string>::const_iterator it=extras.begin();it!=extras.end();++it)
            out->printf("%s: %s\r\n",it->first.c_str(),it->second.c_str());

        out->printf("\r\n");

        if(method!="HEAD")
        {
            if(st.st_size>0)
            {
                char buf[1024];

                ssize_t n;

                while(st.st_size>0 && (n=read(fd,buf,st.st_size>sizeof(buf)?sizeof(buf):st.st_size))>0 && n!=(ssize_t)-1)
                {
                    if(!out->write(buf,(int)n))
                        break;

                    st.st_size-=n;
                }
            }
        }else
            st.st_size=0;

    }else
        { close(fd); return headers(404,false); }

    close(fd);

    if(st.st_size>0)
        return false;

    return true;
}

void http::req::fix_dlna_org_op_for_live(std::string& s)
{
// ..;DLNA.ORG_OP=XX;.. => ..;DLNA.ORG_OP=00;..
    std::string::size_type n=s.find("DLNA.ORG_OP=");

    if(n!=std::string::npos)
    {
        n+=12;

        std::string::size_type m=s.find(';',n);

        if(m!=std::string::npos)
            s.replace(n,m-n,"00");
    }
}

bool http::req::main(void)
{
    if(url.empty() || url[0]!='/' || strstr(url.c_str(),".."))
        return headers(400,false);

    if(url=="/")
        url="/index.html";

    if(!strncmp(url.c_str(),"/ctrl/",6))
    {
        bool rc=false;

        std::string data_out;

        if(method=="POST")
        {
            std::map<std::string,std::string>::const_iterator it=hdrs.find("soapaction");

            if(it!=hdrs.end())
            {
                const std::string& action=it->second;

                std::string::size_type n=action.find('#');

                if(n!=std::string::npos)
                {
                    std::string _interface(url.substr(6)); std::string _method(action.substr(n+1));

                    if(!_method.empty() && _method[_method.length()-1]=='"')
                        _method.resize(_method.length()-1);

                    rc=soap::main(_interface,_method,data,data_out,client_ip);
                }
            }
        }else if(method=="GET")
            rc=soap::main_debug(url.substr(6),args,data_out,client_ip);
        else
            return headers(405,false);

        if(!rc)
            return headers(400,false);
        else
        {
            headers(200,false,data_out.length(),content_type_by_name(".xml"));

            if(!out->write(data_out.c_str(),data_out.length()))
                return false;
        }

        return true;

    }else if(!strncmp(url.c_str(),"/stream/",8))
    {
        if(method!="GET" && method!="HEAD")
            return headers(405,false);

        std::string::size_type n=url.find('.',8);

        if(n==std::string::npos)
            return headers(403,false);

        std::string objid=url.substr(8,n-8);

        if(objid.empty() || objid.length()>32)
            return headers(403,false);

        for(int i=0;i<objid.length();i++)
            if(!isalnum(objid[i]))
                return headers(403,false);

        db::object_t obj;

        {
            db::locker lock;

            if(!db::find_object_by_id(objid,obj))
                return headers(403,false);
        }

        if(obj.url.empty() || obj.objtype<1 || obj.mimecode<0)
            return headers(403,false);

        mime::type_t* t=mime::get_by_id(obj.mimecode);

        if(!t || !t->upnp_proto)
            return headers(403,false);

        std::map<std::string,std::string> extras;

        extras["TransferMode.DLNA.ORG"]="Streaming";

        std::string& ex=extras["ContentFeatures.DLNA.ORG"];

        ex=t->dlna_extras;

        if(!obj.handler.empty())
            fix_dlna_org_op_for_live(ex);

        if(cfg::upnp_hdr_content_disp)
            extras["Content-Disposition"]=utils::format("attachment; filename=\"%s.%s\"",objid.c_str(),t->name);

        if(obj.handler.empty())
            return sendfile(obj.url,extras);
        else
            return live::sendurl(this,obj.url,obj.handler,t->mime,extras);
    }else if(!strncmp(url.c_str(),"/ev/",4))
    {
        if(method=="SUBSCRIBE")
        {
            std::string sid;

            {
                std::map<std::string,std::string>::const_iterator it=hdrs.find("sid");

                if(it!=hdrs.end())
                {
                    const char* p=strstr(it->second.c_str(),"uuid:");
                    if(p)
                        sid=p+5;
                }
            }

            if(sid.empty())
                sid=utils::uuid_gen();

            headers(200,true);

            out->printf("SID: uuid:%s\r\nTIMEOUT: Second-%d\r\n\r\n",sid.c_str(),cfg::upnp_sid_ttl);

            return true;

        }else if(method=="UNSUBSCRIBE")
            return headers(200,false);
        else
            return headers(405,false);
    }else
    {
        if(method!="GET" && method!="HEAD" && method!="POST")
            return headers(405,false);

        // tamplate
        if(utils::is_template(url))
        {
            std::string s; utils::read_template(cfg::http_www_root+url,s);

            headers(200,false,s.length(),content_type_by_name(url));

            if(method!="HEAD")
            {
                if(!out->write(s.c_str(),s.length()))
                    return false;
            }

            return true;
        }else
        {
            std::string::size_type n=url.find_last_of('.');

            if(n!=std::string::npos && !strcmp(url.c_str()+n+1,"lua"))  // lua script
                return scripting::main(*this,cfg::http_www_root+url);
            else
                return sendfile(cfg::http_www_root+url);                // file
        }
    }

    return headers(403,false);
}
