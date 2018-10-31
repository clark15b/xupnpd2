/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __COMMON_H
#define __COMMON_H

#include "compat.h"
#include <string>
#include <map>
#include <stdarg.h>
#include <time.h>
#include <stdio.h>

#ifndef _WIN32
#define log(FMT,ARGS...) utils::trace(utils::log_debug,FMT,ARGS)
#else
#define log(FMT,...) utils::trace(utils::log_debug,FMT,__VA_ARGS__)
#endif /* _WIN32 */

namespace xupnpd
{
    bool all_init_1(int argc,char** argv);

    bool all_init(int argc,char** argv);

    void all_done_1(void);

    void all_done(void);
}

namespace cfg
{
    extern std::string  ssdp_interface;
    extern int          ssdp_broadcast_delay;
    extern int          ssdp_max_age;
    extern std::string  ssdp_group_address;
    extern int          ssdp_group_port;
    extern std::string  ssdp_server;
    extern bool         ssdp_loop;
    extern int          ssdp_ttl;
    extern int          http_port;
    extern int          http_live_port;
    extern int          http_rcv_timeout;
    extern int          http_snd_timeout;
    extern int          http_backlog;
    extern int          http_keep_alive_timeout;
    extern int          http_keep_alive_max;
    extern int          http_max_post_size;
    extern std::string  http_www_root;
    extern std::string  http_templates;
    extern int          live_rcv_timeout;
    extern int          live_snd_timeout;
    extern std::string  upnp_device_name;
    extern std::string  upnp_device_uuid;
    extern int          upnp_sid_ttl;
    extern int          upnp_objid_offset;
    extern std::string  upnp_live_length;
    extern std::string  upnp_live_type;
    extern std::string  upnp_http_type;
    extern std::string  upnp_logo_profile;
    extern bool         upnp_hdr_content_disp;
    extern int          log_level;
    extern bool         disable_dlna_extras;
    extern std::string  db_file;
    extern std::string  media_root;
    extern std::string  io_charset;
    extern std::string  log_file;
    extern std::string  multicast_interface;
    extern bool         daemon_mode;
    extern std::string  http_proxy;
    extern int          startup_delay;

    extern std::string  www_location;
    extern std::string  http_addr;
    extern std::string  uuid;
    extern int          system_update_id;
    extern std::string  version;
    extern std::string  about;

#ifndef _WIN32
    extern pid_t parent_pid;
#endif
}

namespace utils
{
    enum
    {
        log_err         = 1,
        log_info        = 2,
        log_http        = 3,
        log_http_hdrs   = 4,
        log_soap        = 5,
        log_ssdp        = 6,
        log_core        = 7,
        log_debug       = 8
    };

    bool init(void);

    std::string trim(const std::string& s);

    void rm_last_slash(std::string& s);

    std::string uuid_gen(void);

    const std::string& vformat(std::string& dst,const char* fmt,va_list ap);

    const std::string& format(std::string& dst,const char* fmt,...);

    std::string format(const char* fmt,...);

    bool is_trace_needed(int level);

    void trace(int level,const char* fmt,...);

    extern FILE* trace_fp;

    std::string sys_time(time_t timestamp);

    bool read_template(const std::string& from,std::string& to);

    u_int64_t get_file_length(const std::string& s);

    std::string inet_ntoa(struct in_addr addr);

    void do_scan_for_media(void);

    void md5(const std::string& s,std::string& dst);

    int base64encode(unsigned char* src,int nsrc,unsigned char* dst,int ndst);

    bool is_template(const std::string& s);

    bool openlog(void);
}

#endif
