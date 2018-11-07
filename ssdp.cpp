/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "ssdp.h"
#include <string.h>
#include <stdio.h>
#include <list>
#include <ctype.h>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <fcntl.h>
#else /* _WIN32 */
#include <io.h>
#include <winsock2.h>
#include <Ws2ipdef.h>
#endif /* _WIN32 */

namespace ssdp
{
    socket_t incoming_socket=INVALID_SOCKET;
    socket_t outgoing_socket=INVALID_SOCKET;

    sockaddr_in lan_if_sin;
    sockaddr_in mcast_grp_sin;
    sockaddr_in outgoing_socket_sin;

    const char* service_list[]=
    {
        "upnp:rootdevice",
        "urn:schemas-upnp-org:device:MediaServer:1",
        "urn:schemas-upnp-org:service:ContentDirectory:1",
        "urn:schemas-upnp-org:service:ConnectionManager:1",
        "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1",
        NULL
    };

    std::list<std::string> alive_list,byebye_list;

    void add_msg(std::list<std::string>& t,const std::string& nt,const std::string& usn,const std::string& nts)
    {
        char buf[1024];

        int n=snprintf(buf,sizeof(buf),
            "NOTIFY * HTTP/1.1\r\n"
            "HOST: %s:%d\r\n"
            "CACHE-CONTROL: max-age=%d\r\n"
            "LOCATION: %s/dev.xml\r\n"
            "NT: %s\r\n"
            "NTS: %s\r\n"
            "Server: %s\r\n"
            "USN: %s\r\n\r\n",
            cfg::ssdp_group_address.c_str(),cfg::ssdp_group_port,cfg::ssdp_max_age,cfg::www_location.c_str(),nt.c_str(),nts.c_str(),cfg::ssdp_server.c_str(),usn.c_str());

        if(n==-1 || n>=sizeof(buf))
            n=sizeof(buf)-1;

        t.push_back(std::string());
        t.back().assign(buf,n);
    }

    in_addr_t get_lan_if_addr(void) { return lan_if_sin.sin_addr.s_addr; }

    socket_t get_incoming_socket(void) { return incoming_socket; }

    void close_all_sockets(void)
        { closesocket(outgoing_socket); closesocket(incoming_socket); }

    void init_list(std::list<std::string>& t,const std::string& nts)
    {
        add_msg(t,cfg::uuid,cfg::uuid,nts);

        for(int i=0;service_list[i];i++)
            add_msg(t,service_list[i],cfg::uuid+"::"+service_list[i],nts);
    }

    bool get_best_if_addr(void);

    bool get_if_addr(const std::string& ifname,sockaddr_in& sin);

    bool get_if_addr_by_ip(const std::string& ip,sockaddr_in& sin);

    bool mk_outgoing_socket(void);

    bool mk_incoming_socket(void);

    void __process_query(sockaddr_in& from_sin,std::string& s,const std::string& from_str);
}

bool ssdp::init(void)
{
    // find ssdp interface
    if(cfg::ssdp_interface=="auto" && !get_best_if_addr())
        { utils::trace(utils::log_err,"no lan interface found"); return false; }

    if(!get_if_addr(cfg::ssdp_interface,lan_if_sin))
        { utils::trace(utils::log_err,"unknown lan interface: %s",cfg::ssdp_interface.c_str()); return false; }

    // prepare ssdp mcast socket
    get_if_addr_by_ip(cfg::ssdp_group_address,mcast_grp_sin);

    utils::trace(utils::log_info,"ssdp interface: %s, address: %s",cfg::ssdp_interface.c_str(),inet_ntoa(lan_if_sin.sin_addr));

    mcast_grp_sin.sin_port=htons(cfg::ssdp_group_port);

    if(!mk_outgoing_socket())
        { utils::trace(utils::log_err,"unable to create multicast outgoing socket for ssdp exchange"); return false; }

    if(!mk_incoming_socket())
        { utils::trace(utils::log_err,"unable to create multicast incoming socket for ssdp exchange"); return false; }

    utils::format(cfg::http_addr,"%s",inet_ntoa(lan_if_sin.sin_addr));

    utils::format(cfg::www_location,"http://%s:%d",cfg::http_addr.c_str(),cfg::http_port);

    init_list(alive_list,"ssdp:alive");

    init_list(byebye_list,"ssdp:byebye");

    return true;
}

bool ssdp::send_announce(announce_t type)
{
    if(outgoing_socket==INVALID_SOCKET)
        return false;

    std::list<std::string>* t;

    switch(type)
    {
    case alive: t=&alive_list; break;
    case byebye: t=&byebye_list; break;
    default: return false;
    };

    for(std::list<std::string>::const_iterator it=t->begin();it!=t->end();++it)
    {
        const std::string& s=*it;

        ssize_t n=send(outgoing_socket,s.c_str(),s.length(),0);

        if(n!=(ssize_t)-1 && n>0)
            utils::trace(utils::log_ssdp,"ssdp to %s:%d, if=%s, len=%lu",cfg::ssdp_group_address.c_str(),cfg::ssdp_group_port,cfg::ssdp_interface.c_str(),n);
    }

    return true;
}

void ssdp::done(void)
{
    if(incoming_socket!=INVALID_SOCKET)
    {
        ip_mreq mcast_group;
        memset((char*)&mcast_group,0,sizeof(mcast_group));
        mcast_group.imr_multiaddr.s_addr=mcast_grp_sin.sin_addr.s_addr;
        mcast_group.imr_interface.s_addr=lan_if_sin.sin_addr.s_addr;

        setsockopt(incoming_socket,IPPROTO_IP,IP_DROP_MEMBERSHIP,(const char*)&mcast_group,sizeof(mcast_group));

        closesocket(incoming_socket);

        incoming_socket=INVALID_SOCKET;
    }

    if(outgoing_socket!=INVALID_SOCKET)
        { closesocket(outgoing_socket); outgoing_socket=INVALID_SOCKET; }
}

bool ssdp::get_best_if_addr(void)
{
    struct network { in_addr_t addr; in_addr_t mask; };

    network networks[3];
    networks[0].addr=inet_addr("10.0.0.0");    networks[0].mask=inet_addr("255.0.0.0");
    networks[1].addr=inet_addr("172.16.0.0");  networks[1].mask=inet_addr("255.240.0.0");
    networks[2].addr=inet_addr("192.168.0.0"); networks[2].mask=inet_addr("255.255.0.0");

#ifndef _WIN32
    char buf[16384];

    ifconf ifc;
    ifc.ifc_len=sizeof(buf);
    ifc.ifc_buf=buf;

    socket_t s=socket(AF_INET,SOCK_DGRAM,0);

    if(s==INVALID_SOCKET)
        return false;

    if(ioctl(s,SIOCGIFCONF,&ifc)==-1)
        { closesocket(s); return false; }

    closesocket(s);

    if(!ifc.ifc_len)
        return false;

    ifreq* ifr=ifc.ifc_req;

    for(int i=0;i<ifc.ifc_len;)
    {
#if defined(__FreeBSD__) || defined(__APPLE__)
        size_t len=IFNAMSIZ+ifr->ifr_addr.sa_len;
#else
        size_t len=sizeof(*ifr);
#endif
        if(!i)
            cfg::ssdp_interface=ifr->ifr_name;

        if(ifr->ifr_addr.sa_family==AF_INET)
        {
            for(int j=0;j<sizeof(networks)/sizeof(*networks);j++)
            {
                if((((sockaddr_in*)&ifr->ifr_addr)->sin_addr.s_addr & networks[j].mask)==networks[j].addr)
                    { cfg::ssdp_interface=ifr->ifr_name; return true; }
            }
        }

        ifr=(ifreq*)((char*)ifr+len);
        i+=len;
    }

/*
    ifreq ifr[8];

    ifconf ifc;

    ifc.ifc_len=sizeof(ifr);
    ifc.ifc_req=ifr;

    socket_t s=socket(AF_INET,SOCK_DGRAM,0);

    if(s==INVALID_SOCKET)
        return false;

    if(ioctl(s,SIOCGIFCONF,&ifc)==-1)
        { closesocket(s); return false; }

    closesocket(s);

    int count=ifc.ifc_len/sizeof(ifreq);

    if(count<1)
        return false;

    for(int i=0;i<count;i++)
    {
        if(ifr[i].ifr_addr.sa_family==AF_INET)
        {
            for(int j=0;j<sizeof(networks)/sizeof(*networks);j++)
            {
                if((((sockaddr_in*)&ifr[i].ifr_addr)->sin_addr.s_addr & networks[j].mask)==networks[j].addr)
                    { cfg::ssdp_interface=ifr[i].ifr_name; return true; }
            }
        }
    }

    cfg::ssdp_interface=ifr[0].ifr_name;
*/
#else
    socket_t s=WSASocket(AF_INET,SOCK_DGRAM,0,0,0,0);

    if(s!=INVALID_SOCKET)
    {
        INTERFACE_INFO ifl[16];

        unsigned long n;

        if(WSAIoctl(s,SIO_GET_INTERFACE_LIST,0,0,&ifl,sizeof(ifl),&n,0,0)!=SOCKET_ERROR)
        {
            closesocket(s);

            int count=n/sizeof(INTERFACE_INFO);

            for(int i=0;i<n;i++)
            {
                sockaddr_in* addr=(sockaddr_in*)&(ifl[i].iiAddress);
                u_long flags=ifl[i].iiFlags;

                if(flags&IFF_UP && flags&IFF_MULTICAST && !(flags&IFF_LOOPBACK) && !(flags&IFF_POINTTOPOINT))
                {
                    for(int j=0;j<sizeof(networks)/sizeof(*networks);j++)
                    {
                        if((addr->sin_addr.s_addr & networks[j].mask)==networks[j].addr)
                            { cfg::ssdp_interface=inet_ntoa(addr->sin_addr); return true; }
                    }
                }
            }
        }
    }
#endif /* _WIN32 */

    return true;
}

bool ssdp::get_if_addr(const std::string& ifname,sockaddr_in& sin)
{
    sin.sin_family=AF_INET;
    sin.sin_port=0;
    sin.sin_addr.s_addr=inet_addr(ifname.c_str());
#if defined(__FreeBSD__) || defined(__APPLE__)
    sin.sin_len=sizeof(sin);
#endif /* __FreeBSD__ */

    if(sin.sin_addr.s_addr!=INADDR_NONE)
        return true;

#ifdef _WIN32
    if(ifname=="lo")
        { sin.sin_addr.s_addr=inet_addr("127.0.0.1"); return true; }

    return false;
#else
    ifreq ifr;

    int n=snprintf(ifr.ifr_name,IFNAMSIZ,"%s",ifname.c_str());

    if(n==-1 || n>=IFNAMSIZ)
        return false;

    socket_t s=socket(AF_INET,SOCK_DGRAM,0);

    if(s==INVALID_SOCKET)
        return false;

    if(ioctl(s,SIOCGIFADDR,&ifr)==-1 || ifr.ifr_addr.sa_family!=AF_INET)
        { closesocket(s); return false; }

    closesocket(s);

    sin.sin_addr.s_addr=((sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;

    return true;
#endif /* _WIN32 */
}

bool ssdp::get_if_addr_by_ip(const std::string& ip,sockaddr_in& sin)
{
    sin.sin_family=AF_INET;
    sin.sin_port=0;
    sin.sin_addr.s_addr=inet_addr(ip.c_str());
#if defined(__FreeBSD__) || defined(__APPLE__)
    sin.sin_len=sizeof(sin);
#endif /* __FreeBSD__ */
    if(sin.sin_addr.s_addr!=INADDR_NONE)
        return true;

    return false;
}

bool ssdp::mk_outgoing_socket(void)
{
    socket_t sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

    if(sock!=INVALID_SOCKET)
    {
        int mcast_ttl=cfg::ssdp_ttl;

#ifdef _WIN32
        u_long nonb=1;
#else
        int mcast_loop=cfg::ssdp_loop?1:0;
#endif
        socklen_t sin_len=sizeof(outgoing_socket_sin);

        int sndbuf=8192;

#ifdef _WIN32
        setsockopt(sock,SOL_SOCKET,SO_SNDBUF,(const char*)&sndbuf,sizeof(sndbuf));
#else
        setsockopt(sock,SOL_SOCKET,SO_SNDBUF,&sndbuf,sizeof(sndbuf));
#endif

        if(!setsockopt(sock,IPPROTO_IP,IP_MULTICAST_TTL,(const char*)&mcast_ttl,sizeof(mcast_ttl)) &&
#ifndef _WIN32
            !setsockopt(sock,IPPROTO_IP,IP_MULTICAST_LOOP,(const char*)&mcast_loop,sizeof(mcast_loop)) &&
#endif
                !setsockopt(sock,IPPROTO_IP,IP_MULTICAST_IF,(const char*)&lan_if_sin.sin_addr,sizeof(in_addr)) &&
                    !bind(sock,(sockaddr*)&lan_if_sin,sizeof(lan_if_sin)) && !connect(sock,(sockaddr*)&mcast_grp_sin,sizeof(mcast_grp_sin)) &&
                        !getsockname(sock,(sockaddr*)&outgoing_socket_sin,&sin_len) &&
#ifndef _WIN32
                            !fcntl(sock,F_SETFL,O_NONBLOCK)
#else
                            !ioctlsocket(sock,FIONBIO,&nonb)
#endif
                                )
                                    { outgoing_socket=sock; return true; }

        closesocket(sock);
    }

    return false;
}

bool ssdp::mk_incoming_socket(void)
{
    socket_t sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

    if(sock!=INVALID_SOCKET)
    {
        sockaddr_in sin;

        sin.sin_family=AF_INET;
        sin.sin_port=mcast_grp_sin.sin_port;
        // wait ssdp over all interfaces!?!?!?!
        sin.sin_addr.s_addr=INADDR_ANY; /*lan_if_sin.sin_addr.s_addr*/
#if defined(__FreeBSD__) || defined(__APPLE__)
        sin.sin_len=sizeof(sin);
#endif /* __FreeBSD__ */

        ip_mreq mcast_group;
        memset((char*)&mcast_group,0,sizeof(mcast_group));
        mcast_group.imr_multiaddr.s_addr=mcast_grp_sin.sin_addr.s_addr;
        mcast_group.imr_interface.s_addr=lan_if_sin.sin_addr.s_addr;

        int reuse=1; setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));

#ifdef _WIN32
        u_long nonb=1;
        int mcast_loop=cfg::ssdp_loop?1:0;
#endif

        if(!bind(sock,(sockaddr*)&sin,sizeof(sin)) &&
#ifdef _WIN32
            !setsockopt(sock,IPPROTO_IP,IP_MULTICAST_LOOP,(const char*)&mcast_loop,sizeof(mcast_loop)) &&
#endif
                !setsockopt(sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,(const char*)&mcast_group,sizeof(mcast_group)) &&
#ifndef _WIN32
                    !fcntl(sock,F_SETFL,O_NONBLOCK)
#else
                    !ioctlsocket(sock,FIONBIO,&nonb)
#endif
                        )
                            { incoming_socket=sock; return true; }

        closesocket(sock);
    }

    return false;
}

void ssdp::__process_query(sockaddr_in& from_sin,std::string& s,const std::string& from_str)
{
    int idx=0;

    std::map<std::string,std::string> req;

    for(std::string::size_type p1=0,p2;p1!=std::string::npos;p1=p2,idx++)
    {
        std::string ss;

        p2=s.find('\n',p1);

        if(p2!=std::string::npos) { ss=s.substr(p1,p2-p1); p2++; } else ss=s.substr(p1);

        if(!ss.empty() && ss[ss.length()-1]=='\r')
            ss.resize(ss.length()-1);

        if(idx==0 && ss.substr(0,9)!="M-SEARCH ")
            return;

        std::string::size_type p3=ss.find(':');

        if(p3!=std::string::npos)
        {
            for(std::string::size_type p=0;p<p3;p++)
                ss[p]=tolower(ss[p]);

            std::string::size_type p4=ss.find_first_not_of(' ',p3+1);

            if(p4!=std::string::npos)
                req[ss.substr(0,p3)]=ss.substr(p4);
        }
    }

    if(req["man"]!="\"ssdp:discover\"")
        return;

    std::string st=req["st"];

    bool is_ok=false;

    if(st=="ssdp:all" || st==cfg::uuid)
        is_ok=true;

    for(int i=0;!is_ok && service_list[i];i++)
        if(st==service_list[i])
            is_ok=true;

    if(is_ok)
    {
        char buf[1024];

        int n=snprintf(buf,sizeof(buf),
            "HTTP/1.1 200 OK\r\n"
            "CACHE-CONTROL: max-age=%d\r\n"
            "DATE: %s\r\n"
            "EXT:\r\n"
            "LOCATION: %s/dev.xml\r\n"
            "Server: %s\r\n"
            "ST: %s\r\n"
            "USN: %s::%s\r\n\r\n",
            cfg::ssdp_max_age,utils::sys_time(0).c_str(),cfg::www_location.c_str(),cfg::ssdp_server.c_str(),st.c_str(),cfg::uuid.c_str(),st.c_str());

        if(n==-1 || n>=sizeof(buf))
            { n=sizeof(buf)-1; buf[n]=0; }

        n=sendto(outgoing_socket,buf,n,0,(const sockaddr*)&from_sin,sizeof(from_sin));

        if(n>0)
            utils::trace(utils::log_ssdp,"ssdp to %s:%d, if=%s, len=%d",from_str.c_str(),ntohs(from_sin.sin_port),cfg::ssdp_interface.c_str(),n);
    }
}

void ssdp::process_query(void)
{
    ssize_t n;

    char buf[1024];

    sockaddr_in sin;

    socklen_t sin_len=sizeof(sin);

    while((n=recvfrom(incoming_socket,buf,sizeof(buf),0,(sockaddr*)&sin,&sin_len))!=(ssize_t)-1 && n>0)
    {
        if(sin.sin_addr.s_addr!=outgoing_socket_sin.sin_addr.s_addr || sin.sin_port!=outgoing_socket_sin.sin_port)
        {
            std::string s(buf,n);

            std::string from_str=utils::inet_ntoa(sin.sin_addr);

            utils::trace(utils::log_ssdp,"ssdp from %s:%d, len=%lu",from_str.c_str(),(int)ntohs(sin.sin_port),n);

            __process_query(sin,s,from_str);
        }
    }
}
