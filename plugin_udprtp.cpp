/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "compat.h"
#include "common.h"
#include "plugin_udprtp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#else /* _WIN32 */
#include <io.h>
#include <winsock2.h>
#include <Ws2ipdef.h>
#endif /* _WIN32 */

namespace udprtp
{
    bool nake_rtp_packet(const unsigned char*& p,int& len)
    {
        if(len<12)
            return false;

        if( (*p & 0xc0)>>6 !=2 )
            return false;

        bool padding=(*p & 0x20)?true:false;

        bool extras=(*p & 0x10)?true:false;

        int csrcl=((*p & 0x0f) + 3)*4;

        p+=csrcl; len-=csrcl;

        if(len<0)
            return false;

        if(extras)
        {
            if(len<4)
                return false;

            int ehl=p[2]; ehl<<=8; ehl+=p[3]; ehl++; ehl*=4;

            p+=ehl; len-=ehl;

            if(len<0)
                return false;
        }

        if(padding)
        {
            if(len<1)
                return false;

            len-=p[len-1];

            if(len<0)
                return false;
        }

        return true;
    }

    enum { proto_udp=1, proto_rtp=2 };

    in_addr_t get_if_addr(const std::string& ifname);

    int openurl(const std::string& url,int& proto,const std::string& ifaddr);
}

in_addr_t udprtp::get_if_addr(const std::string& ifname)
{
    if(ifname.empty())
        return INADDR_ANY;

    in_addr_t addr=inet_addr(ifname.c_str());

    if(addr!=INADDR_NONE)
        return addr;

#ifdef _WIN32
    if(ifname=="lo")
        return inet_addr("127.0.0.1");

    return INADDR_NONE;
#else
    ifreq ifr;

    int n=snprintf(ifr.ifr_name,IFNAMSIZ,"%s",ifname.c_str());

    if(n==-1 || n>=IFNAMSIZ)
        return INADDR_ANY;

    socket_t s=socket(AF_INET,SOCK_DGRAM,0);

    if(s==INVALID_SOCKET)
        return INADDR_ANY;

    if(ioctl(s,SIOCGIFADDR,&ifr)==-1 || ifr.ifr_addr.sa_family!=AF_INET)
        { closesocket(s); return INADDR_ANY; }

    closesocket(s);

    return ((sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;
#endif /* _WIN32 */
}

int udprtp::openurl(const std::string& url,int& proto,const std::string& ifaddr)
{
    proto=0;

    bool multicast=false;

    bool has_source=false;

    const char* p=url.c_str();

    if(!strncmp(p,"udp://",6))
        proto=proto_udp;
    else if(!strncmp(p,"rtp://",6))
        proto=proto_rtp;
    else
        return -1;

    p+=6;

    const char* p1=strchr(p,'@');

    in_addr_t source_addr;

    if(p1!=NULL)
    {
        multicast=true;

        if(p1>p+1)
        {
            has_source=true;

            source_addr=inet_addr(std::string(p,p1-p).c_str());
        }

        p=++p1;
    }

    const char* p2=strchr(p,':');

    if(!p2)
        return -1;

    in_addr_t addr=inet_addr(std::string(p,p2-p).c_str());

    sockaddr_in sin;
    sin.sin_family=AF_INET;
#ifndef _WIN32
    sin.sin_addr.s_addr=addr;
#else
    sin.sin_addr.s_addr=INADDR_ANY;
#endif /* _WIN32 */
    sin.sin_port=htons(atoi(p2+1));
#if defined(__FreeBSD__) || defined(__APPLE__)
    sin.sin_len=sizeof(sin);
#endif /* __FreeBSD__ */

    socket_t sock=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);

    if(sock!=INVALID_SOCKET)
    {
        int reuse=1; setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));

        if(!bind(sock,(sockaddr*)&sin,sizeof(sin)))
        {
            bool is_ok=true;

            if(multicast)
            {

#ifdef _WIN32
                u_long nonb=1;
                int mcast_loop=cfg::ssdp_loop?1:0;
                setsockopt(sock,IPPROTO_IP,IP_MULTICAST_LOOP,(const char*)&mcast_loop,sizeof(mcast_loop));
#endif

                if(!has_source)
                {
                    ip_mreq mcast_group;

                    memset((char*)&mcast_group,0,sizeof(mcast_group));
                    mcast_group.imr_multiaddr.s_addr=addr;
                    mcast_group.imr_interface.s_addr=get_if_addr(ifaddr);

                    if(setsockopt(sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,(const char*)&mcast_group,sizeof(mcast_group)))
                        is_ok=false;
                }else
                {
                    ip_mreq_source mcast_group;

                    memset((char*)&mcast_group,0,sizeof(mcast_group));
                    mcast_group.imr_multiaddr.s_addr=addr;
                    mcast_group.imr_interface.s_addr=get_if_addr(ifaddr);
                    mcast_group.imr_sourceaddr.s_addr=source_addr;
                    if(setsockopt(sock,IPPROTO_IP,IP_ADD_SOURCE_MEMBERSHIP,(const char*)&mcast_group,sizeof(mcast_group)))
                        is_ok=false;
                }
            }

            if(is_ok)
            {
/*
#ifndef _WIN32
                fcntl(sock,F_SETFL,O_NONBLOCK);
#else
                u_long nonb=1; ioctlsocket(sock,FIONBIO,&nonb);
#endif
*/
                return sock;
            }
        }

        closesocket(sock);
    }

    return -1;
}

void udprtp::sendurl(const std::string& url)
{
    const char* opts=getenv("OPTS");

    if(!opts)
        opts=cfg::multicast_interface.c_str();

    int proto=0;

    int sock=openurl(url,proto,opts);

    if(sock==-1)
        return;

    char buf[2048];

    for(;;)
    {
        ssize_t n=recv(sock,buf,sizeof(buf),0);

        if(n==(ssize_t)-1 || !n)
            break;

        const unsigned char* ptr=(const unsigned char*)buf;

        int len=n;

        if(proto==proto_rtp)
        {
            if(!nake_rtp_packet(ptr,len))
                break;
        }

        n=write(1,ptr,len);

        if(n==(ssize_t)-1 || !n)
            break;
    }

    //setsockopt(incoming_socket,IPPROTO_IP,IP_DROP_MEMBERSHIP,(const char*)&mcast_group,sizeof(mcast_group));

    closesocket(sock);
}
