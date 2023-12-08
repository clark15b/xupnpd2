/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "plugin_hls_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "common.h"

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#else /* _WIN32 */
#include <windows.h>
#include <io.h>
#include <time.h>
#endif /* _WIN32 */

namespace hls
{
    static const char http_tag[]="http://";

    static const char https_tag[]="https://";

    unsigned long str2ulong(const std::string& s)
    {
        unsigned long n=0;

        for(const char* p=s.c_str();*p;p++)
            { int ch=*p; n=n*10+(isdigit(ch)?ch-48:0); }

        return n;
    }

    unsigned long str2usec(const std::string& s)
    {
        unsigned long usec=0;

        std::string::size_type n=s.find('.');

        if(n!=std::string::npos)
        {
            std::string ss=s.substr(n+1); ss.resize(6,'0');

            usec=str2ulong(s.substr(0,n))*1000000 + str2ulong(ss);
        }else
            usec=str2ulong(s)*1000000;

        return usec;
    }

#ifdef _WIN32
    void usleep(long usec)
        { Sleep(usec/1000); }

    int gettimeofday(timeval* tv,void*)
    {
        unsigned long ms=GetTickCount();

        tv->tv_sec=ms/1000;
        tv->tv_usec=(ms%1000)*1000;

        return 0;
    }
#endif /* _WIN32 */

    void tv_now(timeval& tv)
        { gettimeofday(&tv,NULL); }

    unsigned long elapsed_usec(timeval& tv0)
    {
        timeval tv;

        gettimeofday(&tv,NULL);

        return (tv.tv_sec-tv0.tv_sec)*1000000 + (tv.tv_usec-tv0.tv_usec);
    }

    void split_url(const std::string& url,std::string& stream_url,std::string& user_agent,int* stream_id)
    {
        std::string::size_type n=url.find_last_of("#");

        if(n==std::string::npos)
            stream_url=url;
        else
        {
            stream_url=url.substr(0,n);

            for(std::string::size_type p1=n+1,p2;p1!=std::string::npos;p1=p2)
            {
                std::string s;

                p2=url.find_first_of(",;",p1);

                if(p2!=std::string::npos)
                    { s=url.substr(p1,p2-p1); p2++; }
                else
                    s=url.substr(p1);

                std::string::size_type p=s.find('=');

                if(p!=std::string::npos)
                {
                    std::string name=s.substr(0,p);

                    if(name=="user-agent")
                        user_agent=s.substr(p+1);
                    else if(name=="hls-stream-id")
                    {
                        if(stream_id)
                            *stream_id=atoi(s.substr(p+1).c_str());
                    }
                }
            }
        }
    }
}

int hls::vbuf::gets(char* p,int len)
{
    if(len<1)
        return 0;

    len--;

    int bytes_left=len;

    bool quit=false;

    while(!quit && bytes_left>0)
    {
        if(offset>=size)
        {
            ssize_t n;

#ifndef NO_SSL
            if (ssl != NULL) {
                n = ssl::read_with_retry(ssl, buffer, sizeof(buffer));
            }
            else
#endif
            {
#ifndef _WIN32
                n=::read(fd,buffer,sizeof(buffer));
#else
                n=::recv(fd,buffer,sizeof(buffer),0);
#endif
            }

            if(n==(ssize_t)-1 || !n)
            {
                if(bytes_left==len)
                    return -1;
                else
                    break;
            }

            offset=0;

            size=n;
        }

        while(offset<size && bytes_left>0)
        {
            int ch=((const unsigned char*)buffer)[offset++];

            total_read++;

            if(ch=='\r')
                continue;
            else if(ch=='\n')
                { quit=true; break; }
            else
                { *((unsigned char*)p++)=ch; bytes_left--; }
        }
    }

    int n=len-bytes_left;

    *p=0;

    return n;
}

int hls::vbuf::read(char* p,int len)
{
    if(offset<size)
    {
        int n=size-offset;

        if(len<n)
            n=len;

        memcpy(p,buffer+offset,n);

        offset+=n;

        total_read+=n;

        return n;
    }

    ssize_t n;

#ifndef NO_SSL
    if (ssl != NULL) {
        n = ssl::read_with_retry(ssl, p, len);
    }
    else
#endif
    {
#ifndef _WIN32
        n=::read(fd,p,len);
#else
        n=::recv(fd,p,len,0);
#endif
    }

    if(n>0)
        total_read+=n;

    return n;
}

int hls::vbuf::write(const char* p,int len)
{
    int l=0;

    while(l<len)
    {
        ssize_t n;

#ifndef NO_SSL
        if (ssl != NULL) {
            n = ssl::write_with_retry(ssl, p+l, len-l);
        }
        else
#endif
        {
#ifndef _WIN32
            n=::write(fd,p+l,len-l);
#else
            n=::send(fd,p+l,len-l,0);
#endif
        }

        if(n==(ssize_t)-1 || !n)
            return -1;

        l+=n;
    }

    return l;
}

void hls::vbuf::close(void)
{
#ifndef NO_SSL
    if (ssl != nullptr)
        ssl::shutdown(ssl);

    if (ssl_ctx != nullptr)
        ssl::shutdown_context(ssl_ctx);
#endif

    if(fd!=-1)
        { ::closesocket(fd); fd=-1; }

    total_read=0;
}

bool hls::stream::connect(const std::string& host,int port)
{
    sockaddr_in sin;
    sin.sin_family=AF_INET;
    sin.sin_port=htons(port);
    sin.sin_addr.s_addr=inet_addr(host.c_str());

    if(sin.sin_addr.s_addr==INADDR_NONE)
    {
        hostent* he=gethostbyname(host.c_str());

        if(he)
            memcpy((char*)&sin.sin_addr.s_addr,he->h_addr,sizeof(sin.sin_addr.s_addr));
    }

    if(sin.sin_addr.s_addr!=INADDR_NONE)
    {
        fd=socket(sin.sin_family,SOCK_STREAM,0);

        if(fd!=INVALID_SOCKET)
        {
            if(!::connect(fd,(sockaddr*)&sin,sizeof(sin)))
            {
                int on=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,(const char*)&on,sizeof(on));

#ifndef NO_SSL
                if (ssl_ctx != nullptr) {
                    if ((ssl = ssl::create(ssl_ctx, fd, host.c_str()))) {
                        int on=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,(const char*)&on,sizeof(on));

                        return true;
                    } else {
                        ssl::handle_error();
                    }
                }
#endif

                return true;
            }

            close();
        }
    }

    return false;
}

bool hls::stream::_open(const std::string& url,const std::string& range,const std::string& post_data,std::string& location)
{
    close();

    clear();

    static const char cl_tag[]="Content-Length:";

    static const char cr_tag[]="Content-Range:";

    static const char loc_tag[]="Location:";

    std::string proxy_addr,proxy_auth;

    if(!cfg::http_proxy.empty())
    {
        std::string::size_type n=cfg::http_proxy.find('@');

        if(n!=std::string::npos)
            {proxy_auth=cfg::http_proxy.substr(0,n); proxy_addr=cfg::http_proxy.substr(n+1); }
        else
            proxy_addr=cfg::http_proxy;

        if(!proxy_auth.empty())
        {
            char buf[512];

            if(!utils::base64encode((unsigned char*)proxy_auth.c_str(),proxy_auth.length(),(unsigned char*)buf,sizeof(buf)))
                proxy_auth.assign(buf);
            else
                proxy_auth.clear();
        }
    }

    const char* p=url.c_str();

    len=0;

#ifndef NO_SSL
    if(!strncmp(p,http_tag,sizeof(http_tag)-1))
        p+=sizeof(http_tag)-1;
    else if(!strncmp(p,https_tag,sizeof(https_tag)-1))
        p+=sizeof(https_tag)-1;
    else
        return false;
#else
    if(strncmp(p,http_tag,sizeof(http_tag)-1))
        return false;

    p+=sizeof(http_tag)-1;
#endif

    if(!proxy_addr.empty())
        p=proxy_addr.c_str();

    std::string host,resource;

    int port;

    const char* p2=strchr((char*)p,'/');

    if(p2)
        { host.assign(p,p2-p); resource.assign(p2); }
    else
        { host.assign(p); resource="/"; }

    std::string::size_type n=host.find(':');

    if(n!=std::string::npos)
        { port=atoi(host.c_str()+n+1); host=host.substr(0,n); }
    else
#ifndef NO_SSL
    {
        if(!strncmp(url.c_str(),https_tag,sizeof(https_tag)-1))
            port=443;
        else
            port=80;
    }
#else
        port=80;
#endif
    if(!proxy_addr.empty())
        resource=url;

#ifndef NO_SSL
    if(!strncmp(url.c_str(),https_tag,sizeof(https_tag)-1))
    {
        if (!(ssl_ctx = ssl::create_context()))
            return false;

        if (!ssl::set_verify(ssl_ctx, host))
            return false;
    }
#endif

    if(connect(host,port))
    {
        std::string req_data;

        req_data=utils::format("%s %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\n",post_data.empty()?"GET":"POST",
            resource.c_str(),host.c_str(),user_agent.empty()?"Mozilla/5.0":user_agent.c_str());

        if(!range.empty())
            req_data+=utils::format("Range: %s\r\n",range.c_str());

        if(!post_data.empty())
            req_data+=utils::format("Content-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\n",(int)post_data.length());

        if(!proxy_auth.empty())
            req_data+=utils::format("Proxy-Authorization: Basic %s\r\n",proxy_auth.c_str());

        req_data+="\r\n";

        if(write(req_data.c_str(),req_data.length())!=req_data.length())
            { close(); return false; }

        if(!post_data.empty() && write(post_data.c_str(),post_data.length())!=post_data.length())
            { close(); return false; }

        int idx=0;

        std::string code;

        char temp[1024];

        while((gets(temp,sizeof(temp)))>0)
        {
            if(!idx)
            {
                if(strncmp(temp,"HTTP/1.",7))
                    { close(); return false; }

                p=strchr(temp,' ');

                if(!p)
                    { close(); return false; }

                p++;

                char* p2=strchr((char*)p,' ');

                if(!p2)
                    { close(); return false; }

                code.assign(p,p2-p);
            }else
            {
                if(!strncasecmp(temp,cl_tag,sizeof(cl_tag)-1))
                {
                    p=temp+sizeof(cl_tag)-1;

                    while(*p==' ')
                        p++;

                    char* endptr=NULL;

                    long long int l=strtoll(p,&endptr,10);

                    if(!*endptr)
                        len=l;

                    is_len=true;

                }else if(!strncasecmp(temp,cr_tag,sizeof(cr_tag)-1))
                {
                    p=temp+sizeof(cr_tag)-1;

                    while(*p==' ')
                        p++;

                    content_range=p;
                }else if(!strncasecmp(temp,loc_tag,sizeof(loc_tag)-1))
                {
                    p=temp+sizeof(loc_tag)-1;

                    while(*p==' ')
                        p++;

                    location=p;
                }
            }

            idx++;
        }

        if(code.empty() || !(code[0]=='2' || code=="301" || code=="302"))
            { close(); return false; }

        n=url.find_last_of('/');

        if(n!=std::string::npos)
            base_url=url.substr(0,n+1);
        else
            base_url=url+"/";

        total_read=0;

        return true;
    }

    return false;
}

bool hls::stream::open(const std::string& url,const std::string& range,const std::string& post_data)
{
    std::string current_url(url);

    for(int i=0;i<3;i++)
    {
        std::string location;

#ifdef NO_SSL
        if(!strncmp(current_url.c_str(),https_tag,sizeof(https_tag)-1))
            current_url.replace(0,sizeof(https_tag)-1,http_tag);
#endif

        if(!_open(current_url,range,post_data,location))
            return false;

        if(location.empty())
            return true;

        current_url.swap(location);
    }

    return false;
}

int hls::stream::parse_stream_info(int stream_id,chunks_list& chunks)
{
    static const char duration_tag[]=   "#EXT-X-TARGETDURATION:";

    static const char seq_tag[]=        "#EXT-X-MEDIA-SEQUENCE:";

    static const char streaminf_tag[]=  "#EXT-X-STREAM-INF:";

    static const char extinf_tag[]=     "#EXTINF:";

    char buf[1024];

    int idx=0;

    int st=0;

    long base_seq=0;

    unsigned long track_length=0;

    int stream_idx=0;

    std::string stream_url;

#ifndef NO_SSL
    static const char key_tag[] = "#EXT-X-KEY:";
    std::string key_method;
    std::string key_url;
    std::string key_iv;
#endif

    int num=0;

    while((!is_len || total_read<len) && gets(buf,sizeof(buf))!=-1)
    {
        if(!*buf)
            continue;

        if(!idx)
        {
            if(strcmp(buf,"#EXTM3U"))
                return 0;
        }else
        {
            if(!strncmp(buf,extinf_tag,sizeof(extinf_tag)-1))
            {
                st=1;

                char* p=buf+sizeof(extinf_tag)-1;

                char* p2=strchr(p,',');

                if(p2)
                    *p2=0;

                track_length=str2usec(p);

            }if(!strncmp(buf,streaminf_tag,sizeof(streaminf_tag)-1))
                st=2;
            else if(!strncmp(buf,seq_tag,sizeof(seq_tag)-1))
            {
                char* endptr=NULL;

                long l=strtol(buf+sizeof(seq_tag)-1,&endptr,10);

                if(!*endptr)
                    base_seq=l;
            }else if(!strncmp(buf,duration_tag,sizeof(duration_tag)-1))
            {
                chunks.set_target_duration(str2usec(buf+sizeof(duration_tag)-1));
#ifndef NO_SSL
            }else if (!strncmp(buf, key_tag, sizeof(key_tag) - 1))
            {
                char* p = buf + sizeof(key_tag) - 1;

                while (*p == ' ')
                    p++;

                std::string key_info(p);

                size_t start_pos = 0;
                size_t end_pos = 0;

                while (end_pos < key_info.size())
                {
                    while (end_pos < key_info.size() && key_info[end_pos] != ',')
                        end_pos++;

                    std::string token = key_info.substr(start_pos, end_pos - start_pos);
                    size_t equal_pos = token.find('=');

                    if (equal_pos != std::string::npos) {
                        std::string key = token.substr(0, equal_pos);
                        std::string value = token.substr(equal_pos + 1);

                        if (!value.empty() && (value.front() == '"' || value.front() == '\'') && value.front() == value.back()) {
                            value = value.substr(1, value.size() - 2);
                        }

                        if (key == "METHOD") {
                            key_method = value;
                        } else if (key == "URI") {
                            if (!strncmp(value.c_str(), http_tag, sizeof(http_tag) - 1) || !strncmp(value.c_str(), https_tag, sizeof(https_tag) - 1))
                                key_url = value;
                            else
                                key_url = base_url + value;
                        } else if (key == "IV") {
                            key_iv = value;
                        }
                    }

                    start_pos = end_pos + 1;
                    end_pos = start_pos;
                }
#endif
            }else if(*buf!='#')
            {
                if(st==1)
                {
                    long cur_id=base_seq++;

                    long last_id=chunks.get_last_chunk_id();

                    if(last_id==-1 || !cur_id || cur_id>last_id)
                    {
                        std::string url;

                        {
                            int n=strlen(buf);

                            if(!strncmp(buf,http_tag,sizeof(http_tag)-1) || !strncmp(buf,https_tag,sizeof(https_tag)-1))
                                url.assign(buf,n);
                            else
                                { url=base_url; url.append(buf,n); }
                        }
#ifndef NO_SSL
                        chunks.push_back(cur_id,track_length,url,key_method,key_url,key_iv);
#else
                        chunks.push_back(cur_id,track_length,url);
#endif
                    }

                    track_length=0;

                    num++;

                }else if(st==2)
                {
                    if(stream_idx<=stream_id)
                        stream_url=buf;

                    stream_idx++;
                }

                st=0;
            }
        }

        idx++;
    }

    close();

    if(!stream_url.empty())
    {
        stream s(user_agent);

        if(strncmp(stream_url.c_str(),http_tag,sizeof(http_tag)-1) && strncmp(stream_url.c_str(),https_tag,sizeof(https_tag)-1))
            stream_url=base_url+stream_url;

        if(s.open(stream_url))
            num=s.parse_stream_info(stream_id,chunks);
    }

    return num;
}

void http::sendurl(const std::string& url)
{
    using namespace hls;

    std::string stream_url;

    std::string user_agent;

    split_url(url,stream_url,user_agent,NULL);

    stream s(user_agent);

    const char* range=getenv("RANGE");

    if(s.open(stream_url,range?range:""))
    {
        // headers
        fprintf(stdout,"Status: %i\r\nAccept-Ranges: bytes\r\n",s.range().empty()?200:206);

        if(s.is_length_present())
            fprintf(stdout,"Content-Length: %llu\r\n",s.length());

        if(!s.range().empty())
            fprintf(stdout,"Content-Range: %s\r\n",s.range().c_str());

        fprintf(stdout,"\r\n"); fflush(stdout);

        // data

        char buf[4096];

        long long int bytes_left=s.length();

        bool quit=false;

        while(!quit && (!s.is_length_present() || bytes_left>0))
        {
            // read

            int bytes_needed=sizeof(buf);

            if(s.is_length_present())
                bytes_needed=bytes_left>sizeof(buf)?sizeof(buf):bytes_left;

            int n=s.read(buf,bytes_needed);

            if(n<1)
                break;

            if(s.is_length_present())
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
        }

        s.close();
    }
}

int http::fetch(const std::string& url,std::string& dst,const std::string& post_data)
{
    using namespace hls;

    stream s;

    if(!s.open(url,"",post_data))
        return -1;

    char buf[4096];

    long long int bytes_left=s.length();

    bool quit=false;

    while(!quit && (!s.is_length_present() || bytes_left>0))
    {
        // read

        int bytes_needed=sizeof(buf);

        if(s.is_length_present())
            bytes_needed=bytes_left>sizeof(buf)?sizeof(buf):bytes_left;

        int n=s.read(buf,bytes_needed);

        if(n<1)
            break;

        if(s.is_length_present())
            bytes_left-=n;

        // write

        dst.append(buf,n);
    }

    s.close();

    if(bytes_left>0)
        return -3;

    return 0;
}
