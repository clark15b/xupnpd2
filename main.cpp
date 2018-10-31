/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include "common.h"
#include "http.h"
#include "soap.h"
#include "db.h"
#include "mime.h"
#include "ssdp.h"
#include "scan.h"
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>


namespace xupnpd
{
    // signals handler
    volatile int __sig_quit=0;
    volatile int __sig_alrm=0;
    volatile int __sig_chld=0;
    volatile int __sig_usr1=0;

    static int __sig_pipe[2]={-1,-1};

    static void __sig_handler(int n)
    {
        int _errno=errno;

        switch(n)
        {
        case SIGALRM:
            __sig_alrm=1;
            alarm(cfg::ssdp_broadcast_delay);
            break;
        case SIGCHLD:
            __sig_chld=1;
            break;
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
            __sig_quit=1;
            break;
        case SIGUSR1:
            __sig_usr1=1;
            break;
        default:
            break;
        }

        ssize_t rc=write(__sig_pipe[1],"*",1);

        errno=_errno;
    }
}

int main(int argc,char** argv)
{
    if(!xupnpd::all_init(argc,argv))
        goto err;

    {
        signal(SIGUSR1,SIG_IGN);

        // pipe for signal handler
        if(pipe2(xupnpd::__sig_pipe,O_NONBLOCK))
            { utils::trace(utils::log_err,"pipe2: %s",strerror(errno)); goto err; }

        // fork and exit
        if(cfg::daemon_mode)
        {
            pid_t pid=fork();

            if(pid==-1)
                { utils::trace(utils::log_err,"fork: %s",strerror(errno)); goto err; }

            if(pid>0)
                exit(0);

            int fd=open("/dev/null",O_RDWR);

            if(fd!=-1)
            {
                for(int i=0;i<3;i++)
                    dup2(i,fd);
                close(fd);
            }else
            {
                for(int i=0;i<3;i++)
                    close(fd);
            }

            if(utils::trace_fp==stdout || utils::trace_fp==stderr)
                utils::trace_fp=NULL;

            setsid();
        }

        cfg::parent_pid=getpid();

        // signal handlers
        struct sigaction action;
        sigfillset(&action.sa_mask);
        action.sa_flags=0;

        action.sa_handler=SIG_IGN;
        sigaction(SIGHUP,&action,0);
        sigaction(SIGPIPE,&action,0);
        sigaction(SIGUSR2,&action,0);

        action.sa_handler=xupnpd::__sig_handler;
        sigaction(SIGINT,&action,0);
        sigaction(SIGQUIT,&action,0);
        sigaction(SIGALRM,&action,0);
        sigaction(SIGTERM,&action,0);
        sigaction(SIGCHLD,&action,0);
        sigaction(SIGUSR1,&action,0);

        sigset_t sigset;
        sigfillset(&sigset);
        sigprocmask(SIG_BLOCK,&sigset,0);

        int nfds=xupnpd::__sig_pipe[0];

        int ssdp_incoming_socket=ssdp::get_incoming_socket();

        int http_incoming_socket=http::get_incoming_socket();

        if(ssdp_incoming_socket>nfds)
            nfds=ssdp_incoming_socket;

        if(http_incoming_socket>nfds)
            nfds=http_incoming_socket;

        // start SSDP broadcasting
        alarm(cfg::ssdp_broadcast_delay);

        ssdp::send_announce(ssdp::alive);

        // main loop
        while(!xupnpd::__sig_quit)
        {
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(xupnpd::__sig_pipe[0],&fdset);
            FD_SET(ssdp_incoming_socket,&fdset);
            FD_SET(http_incoming_socket,&fdset);

            sigprocmask(SIG_UNBLOCK,&sigset,0);
            int rc=select(nfds+1,&fdset,NULL,NULL,NULL);
            sigprocmask(SIG_BLOCK,&sigset,0);

            if(rc==-1)
            {
                if(errno!=EINTR)
                    break;
            }else if(rc==0)
                continue;

            // signal arrived
            if(FD_ISSET(xupnpd::__sig_pipe[0],&fdset))
                { char buf[256]; while(read(xupnpd::__sig_pipe[0],buf,sizeof(buf))>0); }

            if(xupnpd::__sig_usr1)
                { xupnpd::__sig_usr1=0; utils::scan_for_media(); }

            if(xupnpd::__sig_chld)
            {
                xupnpd::__sig_chld=0;

                pid_t pid;

                while((pid=wait3(0,WNOHANG,0))>0)
                    utils::trace(utils::log_core,"exit child, pid=%u",pid);
            }

            if(xupnpd::__sig_alrm)
                { xupnpd::__sig_alrm=0; ssdp::send_announce(ssdp::alive); }

            if(FD_ISSET(ssdp_incoming_socket,&fdset))
                ssdp::process_query();

            if(FD_ISSET(http_incoming_socket,&fdset))
            {
                int fd;

                sockaddr_in sin;
                socklen_t sin_len=sizeof(sin);

                while((fd=accept(http_incoming_socket,(sockaddr*)&sin,&sin_len))!=-1)
                {
                    int on=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&on,sizeof(on));

                    pid_t pid=fork();

                    if(!pid)
                    {
                        // child process

                        {
                            alarm(0);

                            utils::trace(utils::log_core,"run child, pid=%u",getpid());

                            ssdp::close_all_sockets();
                            close(http_incoming_socket);

                            action.sa_handler=SIG_DFL;
                            sigaction(SIGHUP,&action,0);
                            sigaction(SIGPIPE,&action,0);
                            sigaction(SIGUSR1,&action,0);
                            sigaction(SIGUSR2,&action,0);
                            sigaction(SIGINT,&action,0);
                            sigaction(SIGQUIT,&action,0);
                            sigaction(SIGALRM,&action,0);
                            sigaction(SIGTERM,&action,0);
                            sigaction(SIGCHLD,&action,0);

                            sigprocmask(SIG_UNBLOCK,&sigset,0);

                            fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)&(~O_NONBLOCK));

                            http::stream stream(fd);

                            http::process_query(sin,stream);
                        }

                        exit(0);
                    }else if(pid==-1)
                        utils::trace(utils::log_err,"fork: %s",strerror(errno));

                    close(fd);
                }
            }
        }

        sigprocmask(SIG_UNBLOCK,&sigset,0);
    }

    http::quit=true;

    signal(SIGTERM,SIG_IGN);

    signal(SIGCHLD,SIG_IGN);

    kill(0,SIGTERM);

err:
    xupnpd::all_done();

    return 0;
}
