/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __HTTP_H
#define __HTTP_H

#include "common.h"
#include <string>
#include <map>
#include <stdio.h>

struct sockaddr_in;

namespace http
{
    static const int max_buf_size=1024;

    extern volatile bool quit;

    class ibuffer
    {
    public:
        char buf[max_buf_size]; int offset; int size;

        ibuffer(void):offset(0),size(0) {}

        bool empty(void) { return offset<size?false:true; }
    };

    class obuffer
    {
    public:
        char buf[max_buf_size]; int size;

        obuffer(void):size(0) {}

        int append(const char* p,int len);

        bool empty(void) { return size>0?false:true; }

        void reset(void) { size=0; }
    };

    class stream
    {
    protected:
        socket_t fd; bool is_eof; ibuffer ibuf; obuffer obuf;

        bool __write(const char* p,int len);

        time_t __alarm_time;

        ssize_t os_send(socket_t sockfd,const char* buf,size_t len);

        ssize_t os_recv(socket_t sockfd,char* buf,size_t len);
    public:
        stream(socket_t _fd):fd(_fd),is_eof(false),__alarm_time(0) {}

        ~stream(void);

        void alarm(int n);

        bool printf(const char* fmt,...);

        bool write(const char* p,int len);

        bool flush(void);

        // getc, gets, read - returns EOF
        int __getc(void);

        int gets(char* s,int max);

        int read(char* s,int max);

        socket_t fileno(void) { return fd; }
    };

    class req
    {
    protected:
        void fix_dlna_org_op_for_live(std::string& s);

        void fix_url(void);
    public:
        std::string client_ip;
        std::string method;
        std::string url;
        std::map<std::string,std::string> hdrs;
        std::map<std::string,std::string> args;
        std::string data;

        bool is_keep_alive;     // in/out

        stream* out;

        bool headers(int code,bool extra,u_int64_t content_length=0,const std::string& content_type=std::string());

        bool sendfile(const std::string& filename,const std::map<std::string,std::string>& extras=std::map<std::string,std::string>());

        bool main(void);
    };

    bool init(void);

    socket_t get_incoming_socket(void);

    std::string content_type_by_name(const std::string& filename);

    void split_http_args(const std::string& s,std::map<std::string,std::string>& args);

    void process_query(sockaddr_in& sin,stream& fp);

    void done(void);
}

#endif
