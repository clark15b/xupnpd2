/*
 * Copyright (C) 2015-2016 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include <winsock2.h>
#include <windows.h>
#include "resource.h"
#include "common.h"
#include <list>
#include <stdio.h>
#include <stdarg.h>
#include <io.h>
#include <fcntl.h>
#include "ssdp.h"
#include "http.h"
#include "plugin_hls.h"
#include "plugin_hls_new.h"
#include "plugin_udprtp.h"
#include "plugin_lua.h"
#include "plugin_tsfilter.h"
#include "plugin_hls_decrypt.h"

namespace xupnpd
{
    const char cls[]="xupnpd2";

    const char logfile[]="xupnpd.log";

    const char site[]="http://xupnpd.org";

    char* args[]={ "xupnpd.exe", NULL };

    enum { ev_quit=0, ev_timer=1, ev_child=2 };

    HINSTANCE hInstance=NULL;

    HANDLE events[]={NULL,NULL,NULL};

    HANDLE net_thr=NULL;

    LARGE_INTEGER dtime;

    HANDLE timer=NULL;

    volatile bool __quit=false;

    struct thread
    {
        u_int32_t id; HANDLE thrid; SOCKET fd;

        thread(void):id(0),thrid(NULL),fd(INVALID_SOCKET) {}
    };

    class pool
    {
    protected:
        std::map<u_int32_t,thread> thrs;

        std::list<u_int32_t> finished;

        CRITICAL_SECTION mutex;

        static DWORD WINAPI __main(LPVOID);

        static u_int32_t counter;

    public:
        pool(void)
            { InitializeCriticalSection(&mutex); }

        ~pool(void)
            { DeleteCriticalSection(&mutex); }

        bool start(SOCKET fd)
        {
            if(counter==0xffffffff)
                counter=0;

            thread& thr=thrs[++counter];

            if(thr.fd!=INVALID_SOCKET)
                return false;

            thr.id=counter; thr.fd=fd;

            thr.thrid=CreateThread(NULL,0,__main,&thr,0,NULL);

            if(thr.thrid)
                { utils::trace(utils::log_core,"run child, id=%u",thr.id); return true; }

            thrs.erase(counter);

            return false;
        }

        void wait(u_int32_t id)
        {
            std::map<u_int32_t,thread>::iterator it=thrs.find(id);

            if(it==thrs.end())
                return;

            if(WaitForSingleObject(it->second.thrid,1000)==WAIT_TIMEOUT)
                TerminateThread(it->second.thrid,0);

            CloseHandle(it->second.thrid);

            thrs.erase(it);
        }

        void terminate(void)
        {
            for(std::map<u_int32_t,thread>::iterator it=thrs.begin();it!=thrs.end();++it)
                { utils::trace(utils::log_core,"shutdown child, if=%u",it->first); shutdown(it->second.fd,2); closesocket(it->second.fd); }

            for(std::map<u_int32_t,thread>::iterator it=thrs.begin();it!=thrs.end();++it)
            {
                if(WaitForSingleObject(it->second.thrid,1000)==WAIT_TIMEOUT)
                    TerminateThread(it->second.thrid,0);        // TODO: жестко срубает ожидающий обработчика поток и не завершает конвейер дочерних процессов

                CloseHandle(it->second.thrid);

                utils::trace(utils::log_core,"exit child, thr=%u",it->first);
            }
        }

        void report_exit(u_int32_t id)
        {
            utils::trace(utils::log_core,"report exit child, id=%u",id);

            EnterCriticalSection(&mutex);

            finished.push_back(id);

            SetEvent(events[ev_child]);

            LeaveCriticalSection(&mutex);
        }

        void wait_finished(void)
        {
            EnterCriticalSection(&mutex);

            while(!finished.empty())
            {
                u_int32_t id=finished.front();

                finished.pop_front();

                wait(id);

                utils::trace(utils::log_core,"exit child, id=%u",id);
            }

            LeaveCriticalSection(&mutex);
        }
    }__pool;

    u_int32_t pool::counter=0;

    bool open(void)
    {
        if(((int)ShellExecute(NULL,"open",cfg::www_location.c_str(),NULL,NULL,SW_SHOWNORMAL))>32)
            return true;

        return false;
    }

    bool openlog(void)
    {
        if(((int)ShellExecute(NULL,"open",logfile,NULL,NULL,SW_SHOWNORMAL))>32)
            return true;

        return false;
    }

    bool opensite(void)
    {
        if(((int)ShellExecute(NULL,"open",site,NULL,NULL,SW_SHOWNORMAL))>32)
            return true;

        return false;
    }

    bool openmedia(void)
    {
        if(((int)ShellExecute(NULL,"open",cfg::media_root.c_str(),NULL,NULL,SW_SHOWNORMAL))>32)
            return true;

        return false;
    }

    BOOL WINAPI __con_ctrl(DWORD)
        { SetEvent(events[ev_quit]); return TRUE; }

    int start_handler(const char* lpCmdLine)
    {
        if(!xupnpd::all_init_1(1,args))
            return 0;

        const char* url=getenv("URL");

        if(!url || !*url)
            return 0;

        for(int i=0;i<3;i++)
            _setmode(i,_O_BINARY);

        if(!strcmp(lpCmdLine,"http"))
            http::sendurl(url);
        else if(!strcmp(lpCmdLine,"hls"))
            hls::sendurl(url);
        else if(!strcmp(lpCmdLine,"hls2"))
            hls::sendurl2(url);
        else if(!strcmp(lpCmdLine,"hls3"))
            hls::sendurl3(url);
        else if(!strcmp(lpCmdLine,"hls4"))
            hls_new::sendurl(url);
        else if(!strcmp(lpCmdLine,"hlse"))
            hls_decrypt::sendurl(url);
        else if(!strcmp(lpCmdLine,"udp") || !strcmp(lpCmdLine,"rtp"))
            udprtp::sendurl(url);
        else if(!strcmp(lpCmdLine,"lua"))
            luas::sendurl(url);
        else if(!strcmp(lpCmdLine,"tsfilter"))
            tsfilter::sendurl(url);

        xupnpd::all_done_1();

        return 0;
    }

    HRESULT CALLBACK WindowProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);

    DWORD WINAPI NetThread(LPVOID);

    DWORD WINAPI MainThread(LPVOID);
}

#ifndef _CONSOLE

HRESULT CALLBACK xupnpd::WindowProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    static HANDLE hMainThread=NULL;

    switch(uMsg)
    {
    case WM_CREATE:
        {
            NOTIFYICONDATA nid;

            ZeroMemory((char*)&nid,sizeof(nid));

            nid.cbSize=sizeof(nid);
            nid.hWnd=hWnd;
            nid.uID=IDC_TRAY;
            nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
            nid.uCallbackMessage=WM_TRAY;
            nid.hIcon=LoadIcon(hInstance,MAKEINTRESOURCE(IDR_TRAY_ICON));
//            LoadIconMetric(hInstance,MAKEINTRESOURCE(IDR_TRAY_ICON),LIM_SMALL,&nid.hIcon);
            strcpy_s(nid.szTip,cls);

            Shell_NotifyIcon(NIM_ADD,&nid);

            hMainThread=CreateThread(NULL,0,MainThread,NULL,0,NULL);
        }
        break;

    case WM_DESTROY:
        if(hMainThread)
        {
            SetEvent(events[ev_quit]);

            if(WaitForSingleObject(hMainThread,1000)==WAIT_TIMEOUT)
                TerminateThread(hMainThread,0);

            CloseHandle(hMainThread);
        }

        {
            NOTIFYICONDATA nid;

            ZeroMemory((char*)&nid,sizeof(nid));

            nid.cbSize=sizeof(nid);
            nid.hWnd=hWnd;
            nid.uID=IDC_TRAY;

            Shell_NotifyIcon(NIM_DELETE,&nid);
        }

        PostQuitMessage(0);

        break;

    case WM_TRAY:
        if(wParam==IDC_TRAY)
        {
            if(lParam==WM_LBUTTONUP)
                open();
            else if(lParam==WM_RBUTTONUP)
            {
                POINT pt;

                GetCursorPos(&pt);

                HMENU hMenu=LoadMenu(hInstance,MAKEINTRESOURCE(IDR_TRAY_MENU));

                SetForegroundWindow(hWnd);

                TrackPopupMenuEx(GetSubMenu(hMenu,0),TPM_HORIZONTAL|TPM_LEFTALIGN,pt.x,pt.y,hWnd,NULL);

                DestroyMenu(hMenu);

                PostMessage(hWnd,WM_USER,0,0);
            }
        }
        break;

    case WM_COMMAND:
        if(LOWORD(wParam)==ID_MENU_QUIT)
            DestroyWindow(hWnd);
        else if(LOWORD(wParam)==ID_MENU_OPEN)
            open();
        else if(LOWORD(wParam)==ID_MENU_OPENLOG)
            openlog();
        else if(LOWORD(wParam)==ID_MENU_OPENSITE)
            opensite();
        else if(LOWORD(wParam)==ID_MENU_OPENMEDIA)
            openmedia();
        else if(LOWORD(wParam)==ID_MENU_REFRESH)
            utils::do_scan_for_media();
        break;

    default:
        return DefWindowProc(hWnd,uMsg,wParam,lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInst,HINSTANCE,LPSTR lpCmdLine,int)
{
    if(*lpCmdLine)
        return xupnpd::start_handler(lpCmdLine);

    if(FindWindow(xupnpd::cls,xupnpd::cls))
        return 0;

    WNDCLASS wc;

    ZeroMemory((char*)&wc,sizeof(wc));
    wc.lpfnWndProc=xupnpd::WindowProc;
    wc.hInstance=hInst;
    wc.hCursor=LoadCursor(NULL,MAKEINTRESOURCE(IDC_ARROW));
    wc.hIcon=LoadIcon(hInst,MAKEINTRESOURCE(IDR_TRAY_ICON));
//    LoadIconMetric(hInstance,MAKEINTRESOURCE(IDR_TRAY_ICON),LIM_LARGE,&wc.hIcon);
    wc.lpszClassName=xupnpd::cls;

    xupnpd::hInstance=hInst;

    if(!RegisterClass(&wc))
        return 0;

    utils::trace_fp=fopen(xupnpd::logfile,"w");

    HWND hWnd=CreateWindow(xupnpd::cls,xupnpd::cls,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
        CW_USEDEFAULT,NULL,NULL,hInst,NULL);

    if(!hWnd)
        return 0;

    ShowWindow(hWnd,SW_HIDE);

    UpdateWindow(hWnd);

    MSG msg;

    while(GetMessage(&msg,NULL,0,0))
        { TranslateMessage(&msg); DispatchMessage(&msg); }

    return 0;
}

#else

int main(int argc,char** argv)
{
    if(argc>1)
        return xupnpd::start_handler(argv[1]);

    SetConsoleCtrlHandler(xupnpd::__con_ctrl,TRUE);

    xupnpd::MainThread(NULL);

    return 0;
}

#endif /* _CONSOLE */

DWORD WINAPI xupnpd::MainThread(LPVOID)
{
    if(!xupnpd::all_init(1,args))
        goto err;

    {
        events[ev_quit]=CreateEvent(NULL,TRUE,FALSE,NULL);

        if(!events[ev_quit])
            { utils::trace(utils::log_err,"unable to create stop event"); goto err; }

        events[ev_child]=CreateEvent(NULL,FALSE,FALSE,NULL);

        if(!events[ev_child])
            { utils::trace(utils::log_err,"unable to create child event"); goto err; }

        net_thr=CreateThread(NULL,0,NetThread,NULL,0,NULL);

        if(!net_thr)
            { utils::trace(utils::log_err,"unable to create network thread"); goto err; }

        dtime.QuadPart=cfg::ssdp_broadcast_delay*(-10000000LL);

        events[ev_timer]=CreateWaitableTimer(NULL,FALSE,NULL);

        if(!events[ev_timer] || !SetWaitableTimer(events[ev_timer],&dtime,0,NULL,NULL,0))
            { utils::trace(utils::log_err,"unable to start timer"); goto err; }

        ssdp::send_announce(ssdp::alive);

        while(!__quit)
        {
            DWORD rc=WaitForMultipleObjects(sizeof(events)/sizeof(*events),events,FALSE,INFINITE);

            xupnpd::__pool.wait_finished();

            switch(rc)
            {
            case ev_timer:
                ssdp::send_announce(ssdp::alive);
                SetWaitableTimer(events[ev_timer],&dtime,0,NULL,NULL,0);
                break;
            case ev_child:
                break;
            default:
                __quit=true;
                break;
            }
        }
    }

    if(events[ev_timer])
        CancelWaitableTimer(events[ev_timer]);

err:
    closesocket(http::get_incoming_socket());

    http::quit=true;

    __pool.terminate();

    if(net_thr)
    {
        if(WaitForSingleObject(net_thr,1000)==WAIT_TIMEOUT)
            TerminateThread(net_thr,0);

        CloseHandle(net_thr);
    }

    for(int i=0;i<sizeof(events)/sizeof(*events);i++)
        if(events[i])
            CloseHandle(events[i]);

    xupnpd::all_done();

    return 0;
}

DWORD WINAPI xupnpd::NetThread(LPVOID)
{
    SOCKET ssdp_incoming_socket=ssdp::get_incoming_socket();

    SOCKET http_incoming_socket=http::get_incoming_socket();

    while(!__quit)
    {
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(ssdp_incoming_socket,&fdset);
        FD_SET(http_incoming_socket,&fdset);

        int rc=select(0,&fdset,NULL,NULL,NULL);

        if(rc==SOCKET_ERROR)
            break;

        if(!rc)
            continue;

        if(FD_ISSET(ssdp_incoming_socket,&fdset))
            ssdp::process_query();

        if(FD_ISSET(http_incoming_socket,&fdset))
        {
            SOCKET fd;

            sockaddr_in sin;
            socklen_t sin_len=sizeof(sin);

            while((fd=accept(http_incoming_socket,(sockaddr*)&sin,&sin_len))!=INVALID_SOCKET)
            {
                u_long nonb=0; ioctlsocket(fd,FIONBIO,&nonb);

                if(!xupnpd::__pool.start(fd))
                    closesocket(fd);
            }
        }
    }

    ExitThread(0);

    return 0;
}

DWORD WINAPI xupnpd::pool::__main(LPVOID ctx)
{
    thread* thr=(thread*)ctx;

    {
        int on=1; setsockopt(thr->fd,IPPROTO_TCP,TCP_NODELAY,(const char*)&on,sizeof(on));

        sockaddr_in sin;

        socklen_t sin_len=sizeof(sin);

        if(getpeername(thr->fd,(sockaddr*)&sin,&sin_len)==SOCKET_ERROR || sin.sin_family!=AF_INET)
            { sin.sin_addr.s_addr=INADDR_NONE; sin.sin_port=0; sin.sin_family=AF_INET; }

        http::stream stream(thr->fd);

        http::process_query(sin,stream);
    }

    __pool.report_exit(thr->id);

    ExitThread(0);

    return 0;
}
