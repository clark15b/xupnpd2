/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "common.h"

#ifndef _WIN32
#if defined(__FreeBSD__)
#include <uuid.h>
#elif !defined(NO_LIBUUID)
#include <uuid/uuid.h>
#endif /* __FreeBSD__ */

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#include <sys/file.h>

#endif /* _WIN32 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <list>
#include "mime.h"
#include "version.h"
#include "db.h"
#include "soap.h"
#include "mime.h"
#include "ssdp.h"
#include "http.h"
#include "charset.h"
#include "scripting.h"
#include "live.h"
#include "scan.h"
#include "md5.h"

#ifndef NO_SSL
#include "ssl.h"
#endif

#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <direct.h>
#endif /* _WIN32 */

namespace cfg
{
    std::string ssdp_interface;
    int ssdp_broadcast_delay=0;
    int ssdp_max_age=0;
    std::string ssdp_group_address;
    int ssdp_group_port=0;
    std::string ssdp_server;
    bool ssdp_loop=false;
    int ssdp_ttl=1;
    int http_port=0;
    int http_rcv_timeout=0;
    int http_snd_timeout=0;
    int http_backlog=0;
    int http_keep_alive_timeout=0;
    int http_keep_alive_max=0;
    int http_max_post_size=0;
    std::string http_www_root;
    std::string http_templates;
#ifndef NO_SSL
    bool openssl_verify=true;
    std::string openssl_ca_location;
#endif
    int live_rcv_timeout=0;
    int live_snd_timeout=0;
    std::string upnp_device_name;
    std::string upnp_device_uuid;
    int upnp_sid_ttl=0;
    int upnp_objid_offset=0;
    std::string upnp_live_length;
    std::string upnp_live_type;
    std::string upnp_http_type;
    std::string upnp_logo_profile;
    bool upnp_hdr_content_disp=false;
    int log_level=utils::log_info;
    bool disable_dlna_extras=false;
    std::string db_file;
    std::string media_root;
    std::string io_charset;
    std::string log_file;
    std::string multicast_interface;
    bool daemon_mode=false;
    std::string http_proxy;
    int startup_delay=0;

    std::string www_location;
    std::string http_addr;
    std::string uuid("uuid:");
    int system_update_id=0;

    std::string version(xupnpd_version);
    std::string about;

#ifndef _WIN32
    pid_t parent_pid;
#endif

    enum { tstr=1, tint=2, tbol=3 };

    struct arg_t { const char* name; int type; int min_value; int max_value; void* ptr; };

    static const arg_t args[]=
    {
        { "ssdp_interface",             tstr,   1,      64,     &ssdp_interface                 },
        { "ssdp_broadcast_delay",       tint,   1,      9999,   &ssdp_broadcast_delay           },
        { "ssdp_max_age",               tint,   1,      9999,   &ssdp_max_age                   },
        { "ssdp_group_address",         tstr,   1,      64,     &ssdp_group_address             },
        { "ssdp_group_port",            tint,   1,      65535,  &ssdp_group_port                },
        { "ssdp_server",                tstr,   1,      128,    &ssdp_server                    },
        { "ssdp_loop",                  tbol,   0,      0,      &ssdp_loop                      },
        { "ssdp_ttl",                   tint,   1,      32,     &ssdp_ttl                       },
        { "http_port",                  tint,   1,      65535,  &http_port                      },
        { "http_rcv_timeout",           tint,   0,      9999,   &http_rcv_timeout               },
        { "http_snd_timeout",           tint,   0,      9999,   &http_snd_timeout               },
        { "http_backlog",               tint,   1,      99,     &http_backlog                   },
        { "http_keep_alive_timeout",    tint,   0,      9999,   &http_keep_alive_timeout        },
        { "http_keep_alive_max",        tint,   1,      999999, &http_keep_alive_max            },
        { "http_max_post_size",         tint,   1,      999999, &http_max_post_size             },
        { "http_www_root",              tstr,   1,      512,    &http_www_root                  },
        { "http_templates",             tstr,   0,      1024,   &http_templates                 },
#ifndef NO_SSL
        { "openssl_verify",             tbol,   0,      0,      &openssl_verify                 },
        { "openssl_ca_location",        tstr,   0,      512,    &openssl_ca_location            },
#endif
        { "live_rcv_timeout",           tint,   1,      9999,   &live_rcv_timeout               },
        { "live_snd_timeout",           tint,   1,      9999,   &live_snd_timeout               },
        { "upnp_device_name",           tstr,   0,      64,     &upnp_device_name               },
        { "upnp_device_uuid",           tstr,   0,      64,     &upnp_device_uuid               },
        { "upnp_sid_ttl",               tint,   1,      99999,  &upnp_sid_ttl                   },
        { "upnp_objid_offset",          tint,   1,      2048,   &upnp_objid_offset              },
        { "upnp_live_length",           tstr,   0,      32,     &upnp_live_length               },
        { "upnp_live_type",             tstr,   1,      6,      &upnp_live_type                 },
        { "upnp_http_type",             tstr,   1,      6,      &upnp_http_type                 },
        { "upnp_logo_profile",          tstr,   1,      256,    &upnp_logo_profile              },
        { "upnp_hdr_content_disp",      tbol,   0,      0,      &upnp_hdr_content_disp          },
        { "log_level",                  tint,   -8,     8,      &log_level                      },
        { "disable_dlna_extras",        tbol,   0,      0,      &disable_dlna_extras            },
        { "db_file",                    tstr,   1,      512,    &db_file                        },
        { "media_root",                 tstr,   1,      512,    &media_root                     },
        { "io_charset",                 tstr,   0,      32,     &io_charset                     },
        { "log_file",                   tstr,   0,      512,    &log_file                       },
        { "multicast_interface",        tstr,   0,      64,     &multicast_interface            },
        { "daemon_mode",                tbol,   0,      0,      &daemon_mode                    },
        { "http_proxy",                 tstr,   0,      512,    &http_proxy                     },
        { "sleep",                      tint,   0,      60,     &startup_delay                  },
        { NULL,                         0,      0,      0,      NULL                            }
    };

    std::list<std::string> strings;

    const char* push_str(std::string& s)
    {
        strings.push_back(std::string());

        std::string& ss=strings.back();

        ss.swap(s);

        return ss.c_str();
    }

    bool load(const std::string& s);
}

namespace utils
{
    std::string to_html(const std::string& s);
    std::string to_xml(const std::string& s);

    std::map<std::string,int> templates;

    bool is_template(const std::string& s)
        { return templates.find(s)==templates.end()?false:true; }

    FILE* trace_fp=stdout;

    socket_t trace_fd=INVALID_SOCKET;

    int trace_log_facility=(5<<3);      // LOG_SYSLOG

    using cfg::log_level;
}

bool utils::init(void)
{
#if defined(_WIN32) || defined(NO_LIBUUID)
    srand(time(NULL));
#endif
    return true;
}

std::string utils::trim(const std::string& s)
{
    const char* p1=s.c_str();
    const char* p2=p1+s.length();

    while(*p1 && (*p1==' ' || *p1=='\t'))
        p1++;

    while(p2>p1 && (p2[-1]==' ' || p2[-1]=='\t'))
        p2--;
    return std::string(p1,p2-p1);
}

void utils::rm_last_slash(std::string& s)
{
    if(!s.empty() && (s[s.length()-1]=='/' || s[s.length()-1]=='\\'))
        s.resize(s.length()-1);
}

std::string utils::uuid_gen(void)
{
    static const char uuid_dat[]="xupnpd.uid";

    FILE* fp=fopen(uuid_dat,"rb");

    if(fp)
    {
        char buf[256];

        if(fgets(buf,sizeof(buf),fp))
        {
            char* p=strpbrk(buf,"\r\n");
            if(p)
                *p=0;

            if(strlen(buf)==36)
                { fclose(fp); return std::string(buf,36); }
        }

        fclose(fp);
    }

    std::string uuid;

#if defined(_WIN32) || defined(NO_LIBUUID)
    u_int16_t t[8];

    char buf[64];

    for(int i=0;i<sizeof(t)/sizeof(*t);i++)
        t[i]=rand()&0xffff;

    int n=sprintf(buf,"%.4x%.4x-%.4x-%.4x-%.4x-%.4x%.4x%.4x",t[0],t[1],t[2],t[3],t[4],t[5],t[6],t[7]);

    uuid.assign(buf,n);
#else /* _WIN32 */
#if defined(__FreeBSD__)
    uuid_t _uuid;
    char *p;

    uuid_create(&_uuid,NULL);
    uuid_to_string(&_uuid,&p,NULL);

    uuid.assign(p,36);

    free(p);
#else /* __FreeBSD__ */
    char buf[64];

    uuid_t _uuid;
    uuid_generate(_uuid);

    uuid_unparse_lower(_uuid,buf);

    uuid.assign(buf);
#endif /* __FreeBSD__ */
#endif /* _WIN32 */

    fp=fopen(uuid_dat,"wb");

    if(fp)
    {
        fprintf(fp,"%s\n",uuid.c_str());

        fclose(fp);
    }

    return uuid;
}

const std::string& utils::vformat(std::string& dst,const char* fmt,va_list ap)
{
    char buf[2048]; int n;

    n=vsnprintf(buf,sizeof(buf),fmt,ap);

    if(n==-1 || n>=sizeof(buf))
        n=sizeof(buf)-1;

    dst.assign(buf,n);

    return dst;
}

const std::string& utils::format(std::string& dst,const char* fmt,...)
{
    va_list ap;

    va_start(ap,fmt);

    vformat(dst,fmt,ap);

    va_end(ap);

    return dst;
}

std::string utils::format(const char* fmt,...)
{
    std::string dst;

    va_list ap;

    va_start(ap,fmt);

    vformat(dst,fmt,ap);

    va_end(ap);

    return dst;
}

bool utils::is_trace_needed(int level)
    { return log_level<0?((-level)==log_level?true:false):(level>log_level?false:true); }

void utils::trace(int level,const char* fmt,...)
{
    if(is_trace_needed(level))
    {
        if(trace_fd!=INVALID_SOCKET)
        {
            char buf[512];

            int n=sprintf(buf,"<%i>%s[%i]: ",trace_log_facility|6,"xupnpd2",
#ifdef _WIN32
                GetCurrentProcessId()
#else
                getpid()
#endif /* _WIN32 */
            );

            int max_len=sizeof(buf)-n;

            va_list ap; va_start(ap,fmt); int m=vsnprintf(buf+n,max_len,fmt,ap); va_end(ap);

            if(m==-1 || m>=max_len)
                m=max_len-1;

            send(trace_fd,buf,n+m,MSG_DONTWAIT);

        }else if(trace_fp)
        {
#ifdef _WIN32
            _lock_file(trace_fp);
#else
            flock(fileno(trace_fp),LOCK_EX);
#endif /*  _WIN32 */

            va_list ap; va_start(ap,fmt); vfprintf(trace_fp,fmt,ap); va_end(ap);

            fprintf(trace_fp,"\n");

            fflush(trace_fp);

#ifdef _WIN32
            _unlock_file(trace_fp);
#else
            flock(fileno(trace_fp),LOCK_UN);
#endif /*  _WIN32 */
        }
    }
}

std::string utils::sys_time(time_t timestamp)
{
    if(!timestamp)
        timestamp=time(NULL);

    tm* t=gmtime(&timestamp);

    char buf[128];

    int n=strftime(buf,sizeof(buf),"%a, %d %b %Y %H:%M:%S GMT",t);

    return std::string(buf,n);
}

std::string utils::to_html(const std::string& s)
{
    std::string ss;

    for(const char* p=s.c_str();*p;p++)
        if(*p=='\n')
            ss.append("<br>\n",5);
        else if(*p=='<')
            ss.append("&lt;",4);
        else if(*p=='>')
            ss.append("&gt;",4);
        else
            ss+=*p;

    return ss;
}

std::string utils::to_xml(const std::string& s)
{
    std::string ss;

    for(const char* p=s.c_str();*p;p++)
        if(*p=='\n')
            ss.append("; ",2);
        else if(*p=='<')
            ss.append("&lt;",4);
        else if(*p=='>')
            ss.append("&gt;",4);
        else
            ss+=*p;

    return ss;
}

bool utils::read_template(const std::string& from,std::string& to)
{
    std::map<std::string,std::string> vars;

    vars["device_name"]=cfg::upnp_device_name;
    vars["version"]=xupnpd_version;
    vars["uuid"]=cfg::uuid;
    vars["www_location"]=cfg::www_location;
    vars["interface"]=cfg::ssdp_interface;
    vars["server"]=cfg::ssdp_server;
    vars["www_root"]=cfg::http_www_root;
    vars["media_root"]=cfg::media_root;
    vars["about"]=to_html(cfg::about);
    vars["about_xml"]=to_xml(cfg::about);
    vars["license"]="Free for Home";
//    vars["license"]=to_html("Special for Sultan Dadakhanov <dsultanr@gmail.com>");

    bool rc=false;

    int st=0;

    std::string var; var.reserve(32);

    FILE* sfp=fopen(from.c_str(),"rb");

    if(sfp)
    {
        int ch;

        while((ch=fgetc(sfp))!=EOF)
        {
            switch(st)
            {
            case 0: if(ch=='$') st=1; else to+=ch; break;
            case 1: if(ch=='{') { var.clear(); st=2; } else { st=0; to+='$'; to+=ch;} break;
            case 2:
                if(ch!='}')
                    var+=ch;
                else
                {
                    if(!var.empty())
                    {
                        std::map<std::string,std::string>::const_iterator it=vars.find(var);

                        if(it!=vars.end())
                            { const std::string& s=it->second; to.append(s); }
                    }
                    st=0;
                }
                break;
            }
        }

        rc=true;

        fclose(sfp);
    }

    return rc;
}

u_int64_t utils::get_file_length(const std::string& s)
{
    u_int64_t len=0;

    int fd=open(s.c_str(),O_RDONLY|O_LARGEFILE|O_BINARY);

    if(fd!=-1)
    {
        off64_t l=lseek64(fd,0,SEEK_END);
        if(l!=(off64_t)-1)
            len=l;

        close(fd);
    }

    return len;
}

std::string utils::inet_ntoa(struct in_addr addr)
{
    char buf[128];

#ifndef _WIN32
    if(inet_ntop(AF_INET,&addr,buf,sizeof(buf)))
#else
    if(InetNtop(AF_INET,&addr,buf,sizeof(buf)))
#endif
        return buf;

    return "undef";
}

void utils::do_scan_for_media(void)
{
#ifndef _WIN32
    kill(cfg::parent_pid,SIGUSR1);
#else
    scan_for_media();
#endif
}

void utils::md5(const std::string& s,std::string& dst)
{
    using namespace md5;

    unsigned char md[16];

    {
        MD5_CTX ctx;
        MD5_Init(&ctx);
        MD5_Update(&ctx,(const unsigned char*)s.c_str(),s.length());
        MD5_Final(md,&ctx);
    }

    char buf[sizeof(md)*2];

    static const char hex[]="0123456789abcdef";

    for(int i=0,j=0;i<sizeof(md);i++)
        { buf[j++]=hex[(md[i]>>4)&0x0f]; buf[j++]=hex[md[i]&0x0f]; }

    dst.assign(buf,sizeof(buf));
}

int utils::base64encode(unsigned char* src,int nsrc,unsigned char* dst,int ndst)
{
    int x=nsrc*4; int y=x%3; int nndst=x/3+(y?4-y:0);

    if(ndst<=nndst)
        return -1;

    static const char tbl[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    x=nsrc/3;

    for(unsigned long i=0;i<x;i++)
    {
        dst[0]=tbl[(src[0]>>2)&0x3f]; dst[1]=tbl[(src[1]>>4&0x0f)|(src[0]<<4&0x30)];
        dst[2]=tbl[(src[1]<<2&0x3c)|(src[2]>>6&0x03)]; dst[3]=tbl[src[2]&0x3f];
        src+=3; dst+=4;
    }

    nsrc-=x*3;
    if(nsrc==1)
    {
        dst[0]=tbl[(src[0]>>2)&0x3f]; dst[1]=tbl[(src[0]<<4&0x30)];
        dst[2]='='; dst[3]='='; dst+=4;
    }else if(nsrc==2)
    {
        dst[0]=tbl[(src[0]>>2)&0x3f]; dst[1]=tbl[(src[1]>>4&0x0f)|(src[0]<<4&0x30)];
        dst[2]=tbl[(src[1]<<2&0x3c)]; dst[3]='='; dst+=4;
    }

    *dst=0;

    return 0;
}

bool utils::openlog(void)
{
    if(cfg::log_file.substr(0,6)=="udp://")
    {
        sockaddr_in sin; sin.sin_family=AF_INET;

        std::string s=cfg::log_file.substr(6);

        std::string::size_type n=s.find('/');

        if(n!=std::string::npos)
        {
            std::string ss=s.substr(n+1); s=s.substr(0,n);

            if(ss=="local0")      trace_log_facility=(16<<3);
            else if(ss=="local1") trace_log_facility=(17<<3);
            else if(ss=="local2") trace_log_facility=(18<<3);
            else if(ss=="local3") trace_log_facility=(19<<3);
            else if(ss=="local4") trace_log_facility=(20<<3);
            else if(ss=="local5") trace_log_facility=(21<<3);
            else if(ss=="local6") trace_log_facility=(22<<3);
            else if(ss=="local7") trace_log_facility=(23<<3);
            else if(ss=="daemon") trace_log_facility=(3<<3);
            else if(ss=="user")   trace_log_facility=(1<<3);
        }

        n=s.find(':');

        if(n!=std::string::npos)
            { sin.sin_addr.s_addr=inet_addr(s.substr(0,n).c_str()); sin.sin_port=htons(atoi(s.substr(n+1).c_str())); }
        else
            { sin.sin_addr.s_addr=inet_addr(s.c_str()); sin.sin_port=htons(514); }

        socket_t fd=socket(AF_INET,SOCK_DGRAM,0);

        if(fd!=INVALID_SOCKET)
        {
            if(!connect(fd,(sockaddr*)&sin,sizeof(sin)))
                { trace_fd=fd; return true; }

            closesocket(fd);
        }

        return false;

    }else if(!cfg::log_file.empty() && (utils::trace_fp==stdout || utils::trace_fp==stderr))
    {
        FILE* fp=fopen(cfg::log_file.c_str(),"w");

        if(fp)
            { utils::trace_fp=fp; return true; }
    }

    return false;
}

bool cfg::load(const std::string& s)
{
    using namespace utils;
    using namespace xupnpd;

    FILE* fp=fopen(s.c_str(),"rb");

    if(!fp)
        { utils::trace(utils::log_err,"can't open config file '%s' - %s",s.c_str(),strerror(errno)); return false; }

    std::map<std::string,std::string> _p;

    char buf[512];

    while(fgets(buf,sizeof(buf),fp))
    {
        char* p=strpbrk(buf,"#\r\n");

        if(p)
            *p=0;

        p=strchr(buf,'=');

        if(p)
        {
            *p=0; p++;

            std::string name=trim(buf); std::string val=trim(p);

            const char* nm="";

            bool err=false;

            if(!strncmp(name.c_str(),"mime_type_",10))
            {
                nm=name.c_str()+10;

                mime::type_t* t=mime::get_by_name(nm);

                if(!t)
                    err=true;

                t->mime=push_str(val);
            }else if(!strncmp(name.c_str(),"upnp_proto_",11))
            {
                nm=name.c_str()+11;

                mime::type_t* t=mime::get_by_name(nm);

                if(!t)
                    err=true;

                t->upnp_proto=push_str(val);
            }else if(!strncmp(name.c_str(),"dlna_extras_",12))
            {
                nm=name.c_str()+12;

                mime::type_t* t=mime::get_by_name(nm);

                if(!t)
                    err=true;

                t->dlna_extras=push_str(val);
            }else
                _p[name]=val;

            if(err)
                { utils::trace(utils::log_err,"unknown file type '%s'",nm); fclose(fp); return false; }
        }
    }

    fclose(fp);

    for(int i=0;args[i].name;i++)
    {
        const arg_t& t=args[i];

        const std::string& s=_p[t.name];

        bool is_err=false;

        switch(t.type)
        {
        case tstr:
            if(s.length()<t.min_value || s.length()>t.max_value)
                is_err=true;
            else
                ((std::string*)t.ptr)->assign(s);

            if(is_err)
                { utils::trace(utils::log_err,"invalid string option %s, valid length: %d..%d",t.name,t.min_value,t.max_value); return false; }

            break;
        case tint:
            if(s.empty())
                is_err=true;
            else
            {
                const char* p=s.c_str();

                if(*p=='-')
                    p++;

                for(;*p;p++)
                    if(!isdigit(*p))
                        { is_err=true; break; }
            }

            if(!is_err)
                *((int*)t.ptr)=atoi(s.c_str());

            if(*((int*)t.ptr)<t.min_value || *((int*)t.ptr)>t.max_value)
                is_err=true;

            if(is_err)
                { utils::trace(utils::log_err,"invalid integer option %s, valid range: %d..%d",t.name,t.min_value,t.max_value); return false; }

            break;
        case tbol:
            if(s=="true" || s=="1" || s=="on")
                *((bool*)t.ptr)=true;
            else if(s=="false" || s=="0" || s=="off")
                *((bool*)t.ptr)=false;
            else
                is_err=true;

            if(is_err)
                { utils::trace(utils::log_err,"invalid boolean option %s, valid values: true/false, on/off, 1/0",t.name); return false; }

            break;
        }
    }

    return true;
}

bool xupnpd::all_init_1(int argc,char** argv)
{
    // change directory to the executable
    {
        std::string s(argv[0]);

        std::string::size_type n=s.find_last_of("\\/");

        if(n!=std::string::npos)
        {
            int rc=chdir(s.substr(0,n).c_str());
        }
    }

    if(!utils::init())
        return false;

    if(!mime::init())
        return false;

    if(!scripting::init())
        return false;

#ifdef _WIN32
    WSADATA wsaData;

    if(WSAStartup(MAKEWORD(2,2),&wsaData))
        { utils::trace(utils::log_err,"WinSock 2.2 initialize fail"); return false; }
#endif

    if(!scripting::init())
        return false;

    // parse config
    if(!cfg::load(argc>1?argv[1]:"xupnpd.cfg"))
        return false;

    utils::openlog();

    return true;
}

bool xupnpd::all_init(int argc,char** argv)
{
    utils::format(cfg::about,
        "xupnpd-%s\n"
        "Copyright (C) 2015-2018 Anton Burdinuk <clark15b@gmail.com>\n"
        "All rights reserved\n"
        "Proprietary software\n",cfg::version.c_str());

    utils::trace(utils::log_info,"%s",cfg::about.c_str());

    all_init_1(argc,argv);

    if(cfg::upnp_device_uuid.empty())
        cfg::upnp_device_uuid=utils::uuid_gen();

    if(cfg::upnp_device_name.empty())
    {
        cfg::upnp_device_name="xupnpd2 [";
        char buf[256]=""; gethostname(buf,sizeof(buf));
        cfg::upnp_device_name.append(buf);
        cfg::upnp_device_name+=']';
    }

    utils::rm_last_slash(cfg::http_www_root);
    utils::rm_last_slash(cfg::media_root);

    {
        for(std::string::size_type p1=0,p2;p1!=std::string::npos;p1=p2)
        {
            p2=cfg::http_templates.find_first_of(";,",p1);

            if(p2!=std::string::npos)
                { utils::templates[cfg::http_templates.substr(p1,p2-p1)]=1; p2++; }
            else
                utils::templates[cfg::http_templates.substr(p1)]=1;
        }
    }

    if(!cfg::io_charset.empty())
    {
        if(!charset::set(cfg::io_charset))
            { utils::trace(utils::log_err,"unknown charset '%s'",cfg::io_charset.c_str()); return false; }
    }

    cfg::uuid+=cfg::upnp_device_uuid;

#ifdef _WIN32
    if(cfg::startup_delay>0)
        Sleep(1000*cfg::startup_delay);
#else
    if(cfg::startup_delay>0)
        sleep(cfg::startup_delay);
#endif

    if(!ssdp::init())
        return false;

    if(!http::init())
        return false;

    if(!live::init())
        return false;

#ifndef NO_SSL
    if(!ssl::init())
        return false;
#endif

    utils::trace(utils::log_info,"location: %s",cfg::www_location.c_str());

    if(cfg::disable_dlna_extras)
        mime::disable_dlna_extras();

    if(!db::init())
        return false;

    int n=utils::scan_for_media();

    if(!soap::init())
        return false;

    return true;
}

void xupnpd::all_done_1(void)
{
    mime::done();

    scripting::done();

    if(utils::trace_fd!=INVALID_SOCKET)
        closesocket(utils::trace_fd);

#ifdef _WIN32
    WSACleanup();
#endif /* _WIN32 */

    if(utils::trace_fp && utils::trace_fp!=stdout && utils::trace_fp!=stderr)
        fclose(utils::trace_fp);
}

void xupnpd::all_done(void)
{
    ssdp::send_announce(ssdp::byebye);

    db::done();

    ssdp::done();

    soap::done();

    http::done();

    live::done();

#ifndef NO_SSL
    ssl::done();
#endif

    utils::trace(utils::log_info,"bye.");

    all_done_1();
}


