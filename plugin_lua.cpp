/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "plugin_lua.h"
#include "plugin_hls.h"
#include "luajson.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "luacompat.h"

namespace luas
{
    void reglibs(lua_State* st);

    char* url_decode(char* s);

    int lua_urldecode(lua_State* L);

    int lua_fetch(lua_State* L);

    int lua_trace_level(lua_State* L,int level);

    int lua_trace(lua_State* L);

    int lua_debug(lua_State* L);
}

char* luas::url_decode(char* s)
{
    static const unsigned char t[256]=
    {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    char* ptr=s;

    unsigned char* d=(unsigned char*)s;

    while(*s)
    {
        if(*s=='+')
            *d=' ';
        else if(*s=='%')
        {
            if(!s[1] || !s[2])
                break;

            unsigned char c1=t[s[1]];
            unsigned char c2=t[s[2]];

            if(c1==0xff || c2==0xff)
                break;
            *d=((c1<<4)&0xf0)|c2;
            s+=2;
        }else if((unsigned char*)s!=d)
            *d=*s;

        s++;
        d++;
    }

    *d=0;

    return ptr;
}

int luas::lua_urldecode(lua_State* L)
{
    size_t l=0;

    const char* s=lua_tolstring(L,1,&l);

    if(!s)
    {
        lua_pushstring(L,"");

        return 1;
    }

    char* p=(char*)malloc(l+1);
    if(p)
    {
        memcpy(p,s,l+1);

        lua_pushstring(L,url_decode(p));

        free(p);
    }else
        lua_pushstring(L,"");

    return 1;
}

int luas::lua_fetch(lua_State* L)
{
    std::string data;

    const char* url=lua_tostring(L,1);

    std::string post_data;

    if(lua_gettop(L)>1)
    {
        const char* p=NULL;

        size_t l=0;

        p=lua_tolstring(L,2,&l);

        if(p && l>0)
            post_data.assign(p,l);
    }

    if(url)
        http::fetch(url,data,post_data);

    lua_pushlstring(L,data.c_str(),data.length());

    return 1;
}

int luas::lua_trace_level(lua_State* L,int level)
{
    char ss[1024]; int n=0;

    int count=lua_gettop(L);

    lua_getglobal(L,"tostring");

    for(int i=1;i<=count;i++)
    {
        lua_pushvalue(L,-1);

        lua_pushvalue(L,i);

        lua_call(L,1,1);

        size_t l=0;

        const char* s=lua_tolstring(L,-1,&l);

        if(s)
        {
            int m=sizeof(ss)-n;

            if(l>=m)
                l=m-1;

            memcpy(ss+n,s,l); n+=l;

            if(i<count)
            {
                if(n<sizeof(ss)-1)
                    ss[n++]=' ';
            }

        }
        lua_pop(L,1);
    }

    ss[n]=0;

    utils::trace(level,"%s",ss);

    return 0;
}

int luas::lua_trace(lua_State* L)
    { return lua_trace_level(L,utils::log_info); }

int luas::lua_debug(lua_State* L)
    { return lua_trace_level(L,utils::log_debug); }

void luas::reglibs(lua_State* st)
{
    luaL_openlibs(st);

    luaopen_luajson(st);

    static const luaL_Reg lib_utils[]=
    {
        { "fetch",      lua_fetch },
        { "urldecode",  lua_urldecode },
        { "trace",      lua_trace },
        { "debug",      lua_debug },
        { NULL,         NULL }
    };

    luaL_register(st,"utils",lib_utils);

    lua_register(st,"print",lua_trace);
}

void luas::sendurl(const std::string& url)
{
    const char* opts=getenv("OPTS");

    if(opts)
    {
        lua_State* st=lua_open();

        if(st)
        {
            reglibs(st);

            if(!luaL_loadfile(st,opts))
                lua_pcall(st,0,0,0);

            lua_close(st);
        }
    }
}

std::string luas::translate_url(const std::string& url_translator,const std::string& url,const std::string& method)
{
    std::string real_url;

    lua_State* st=lua_open();

    if(st)
    {
        reglibs(st);

        if(!luaL_loadfile(st,"xupnpd.lua") && !lua_pcall(st,0,0,0))
        {
            lua_getglobal(st,(url_translator+"_translate_url").c_str());

            if(lua_type(st,-1)==LUA_TFUNCTION)
            {
                lua_pushlstring(st,url.c_str(),url.length());

                lua_pushlstring(st,method.c_str(),method.length());

                if(!lua_pcall(st,2,1,0))
                {
                    size_t n=0;

                    const char* p=lua_tolstring(st,-1,&n);

                    if(p && n>0)
                        real_url.assign(p,n);

                    lua_pop(st,1);
                }
            }else
                lua_pop(st,1);
        }

        lua_close(st);
    }

    return real_url;
}
