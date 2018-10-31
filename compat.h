/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __COMPAT_H
#define __COMPAT_H

#ifdef _WIN32
typedef unsigned __int8 u_int8_t;
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
typedef unsigned __int64 u_int64_t;
typedef unsigned __int64 off64_t;
typedef int             ssize_t;
typedef int             socklen_t;
typedef unsigned long   in_addr_t;
typedef unsigned int    socket_t;
#define lseek64         _lseeki64
#define strncasecmp     _strnicmp
#define strcasecmp      _stricmp
#define chdir           _chdir
#define stat64          _stat64
#define fstat64         _fstat64

#if _MSC_VER<1900
#define snprintf        _snprintf
#endif /* _MSC_VER */

#define O_LARGEFILE     0
#define OS_SLASH        '\\'
#define MSG_DONTWAIT    0
#endif /* _WIN32 */

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <arpa/inet.h>
#define O_LARGEFILE     0
#define lseek64         lseek
#define stat64          stat
#define fstat64         fstat
typedef off_t           off64_t;
#endif /*  __FreeBSD__  */

#if defined(__APPLE__)
#include <unistd.h>
#define O_LARGEFILE     0
typedef off_t           off64_t;
#define lseek64         lseek
int pipe2(int*,int);
#endif /* __APPLE__ */


#ifndef _WIN32
#include <sys/types.h>
typedef int             socket_t;
#define closesocket     close
#define INVALID_SOCKET  -1
#define SOCKET_ERROR    -1
#define OS_SLASH        '/'
#define O_BINARY        0
#endif /* _WIN32 */

#if !defined(__FreeBSD__)
struct in_addr;
#endif /*  __FreeBSD__  */

#endif
