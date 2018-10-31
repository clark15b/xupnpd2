/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __LUAJSON_H
#define __LUAJSON_H

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

int luaopen_luajson(lua_State* L);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /*__LUAJSON_H*/
