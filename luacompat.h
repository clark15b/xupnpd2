/* 
 * Copyright (C) 2011-2015 Anton Burdinuk
 * clark15b@gmail.com
 * https://tsdemuxer.googlecode.com/svn/trunk/xupnpd
*/

#ifndef __LUA_COMPAT_H
#define __LUA_COMPAT_H

extern "C"
{
#include <lua.h>
}

#if LUA_VERSION_NUM > 501
#define lua_open luaL_newstate

inline void luaL_register(lua_State* L,const char* libname,const luaL_Reg* l)
{
    if(!libname)
        luaL_setfuncs(L,l,0);
    else
        { lua_createtable(L,0,0); luaL_setfuncs(L,l,0); lua_setglobal(L,libname); }
}

#endif

#endif /* __LUA_COMPAT_H */
