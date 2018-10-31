/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "scripting.h"

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <list>
#include <ctype.h>
#include "db.h"
#include "scan.h"

bool scripting::init(void)
    { return true; }

void scripting::done(void) { }

namespace scripting
{
    static const char LUA_HTTP[]="http::req";

    struct httpreq
    {
        http::req* req;
        std::string* outbuf;
        int* status;
        std::string* content_type;
        std::list<std::string>* headers;
    };

    void lua_my_print(lua_State* L,std::string& ss);
    int lua_print(lua_State* L);
    int lua_httpreq_gc(lua_State* L);
    int lua_httpreq_tostring(lua_State* L);
    int lua_httpreq_print(lua_State* L);
    int lua_httpreq_status(lua_State* L);
    int lua_httpreq_content_type(lua_State* L);
    int lua_httpreq_add_header(lua_State* L);
    int lua_httpreq_data(lua_State* L);
    int lua_httpreq_data_length(lua_State* L);
    int lua_scan_for_media(lua_State* L);
    int lua_browse(lua_State* L);
}


bool scripting::main(http::req& req,const std::string& filename)
{
    std::string outbuf;
    int status=500;
    std::string content_type;
    std::list<std::string> headers;

    lua_State* st=lua_open();

    if(!st)
        { utils::trace(utils::log_err,"LUA: unable to open lua engine"); req.headers(500,false); return true; }

    luaL_openlibs(st);

    static const luaL_Reg __http[]=
    {
        { "__gc",               lua_httpreq_gc },
        { "__tostring",         lua_httpreq_tostring },
        { "print",              lua_httpreq_print },
        { "out",                lua_httpreq_print },
        { "write",              lua_httpreq_print },
        { "status",             lua_httpreq_status },
        { "status",             lua_httpreq_status },
        { "content_type",       lua_httpreq_content_type },
        { "add_header",         lua_httpreq_add_header },
        { "data",               lua_httpreq_data },
        { "data_length",        lua_httpreq_data_length },
        {NULL, NULL}
    };

    httpreq* p=(httpreq*)lua_newuserdata(st,sizeof(httpreq));
    p->req=&req;
    p->outbuf=&outbuf;
    p->status=&status;
    p->content_type=&content_type;
    p->headers=&headers;

    luaL_newmetatable(st,LUA_HTTP);
    lua_pushvalue(st,-1);
    lua_setfield(st,-2,"__index");
    luaL_register(st,0,__http);

    lua_setmetatable(st,-2);
    lua_setglobal(st,"http");

    lua_register(st,"print",lua_print);
    lua_register(st,"scan_for_media",lua_scan_for_media);
    lua_register(st,"browse",lua_browse);

    for(std::map<std::string,std::string>::const_iterator it=req.hdrs.begin();it!=req.hdrs.end();++it)
    {
        std::string name("HTTP_");
        for(const char* p=it->first.c_str();*p;p++)
            { if(*p=='-') name+='_'; else name+=toupper(*p); }

        lua_pushlstring(st,it->second.c_str(),it->second.length());
        lua_setglobal(st,name.c_str());
    }

    lua_newtable(st);
    for(std::map<std::string,std::string>::const_iterator it=req.args.begin();it!=req.args.end();++it)
    {
        lua_pushlstring(st,it->first.c_str(),it->first.length());
        lua_pushlstring(st,it->second.c_str(),it->second.length());
        lua_rawset(st,-3);
    }
    lua_setglobal(st,"args");

    lua_pushstring(st,cfg::http_www_root.c_str()); lua_setglobal(st,"DOCUMENT_ROOT");
    lua_pushstring(st,req.client_ip.c_str()); lua_setglobal(st,"REMOTE_ADDR");
    lua_pushstring(st,req.method.c_str()); lua_setglobal(st,"REQUEST_METHOD");
    lua_pushstring(st,req.url.c_str()); lua_setglobal(st,"REQUEST_URI");
    lua_pushstring(st,filename.c_str()); lua_setglobal(st,"SCRIPT_FILENAME");
    lua_pushstring(st,cfg::http_addr.c_str()); lua_setglobal(st,"SERVER_NAME");
    lua_pushinteger(st,cfg::http_port); lua_setglobal(st,"SERVER_PORT");

    if(luaL_loadfile(st,filename.c_str()) || lua_pcall(st,0,0,0))
    {
        std::string s(lua_tostring(st,-1));

        lua_pop(st,1);

        utils::trace(utils::log_err,"LUA: %s",s.c_str());

        req.headers(500,false);

        return true;
    }

    lua_close(st);

    req.headers(status,true,outbuf.length(),content_type);

    for(std::list<std::string>::const_iterator it=headers.begin();it!=headers.end();++it)
        req.out->printf("%s\r\n",it->c_str());

    req.out->printf("\r\n");

    if(!req.out->write(outbuf.c_str(),outbuf.length()))
        return false;

    return true;
}

void scripting::lua_my_print(lua_State* L,std::string& ss)
{
    int count=lua_gettop(L);

    lua_getglobal(L,"tostring");

    for(int i=1,j=0;i<=count;i++)
    {
        if(lua_type(L,i)!=LUA_TUSERDATA)
        {
            lua_pushvalue(L,-1);
            lua_pushvalue(L,i);
            lua_call(L,1,1);

            size_t l=0;
            const char* s=lua_tolstring(L,-1,&l);
            if(s && l>0)
                { if(j++) ss+=' '; ss.append(s,l); }

            lua_pop(L,1);
        }
    }
}

int scripting::lua_print(lua_State* L)
{
    std::string s;

    lua_my_print(L,s);

    if(!s.empty())
        utils::trace(utils::log_err,"%s",s.c_str());

    return 0;
}

int scripting::lua_httpreq_print(lua_State* L)
{
    httpreq* p=(httpreq*)luaL_checkudata(L,1,LUA_HTTP);

    lua_my_print(L,*(p->outbuf));

    return 0;
}

int scripting::lua_httpreq_gc(lua_State* L)
    { return 0; }

int scripting::lua_httpreq_tostring(lua_State* L)
    { lua_pushstring(L,LUA_HTTP); return 1; }

int scripting::lua_httpreq_status(lua_State* L)
{
    *((httpreq*)luaL_checkudata(L,1,LUA_HTTP))->status=luaL_checkint(L,2);

    return 0;
}

int scripting::lua_httpreq_content_type(lua_State* L)
{
    httpreq* p=(httpreq*)luaL_checkudata(L,1,LUA_HTTP);

    *p->status=200;

    *p->content_type=luaL_checkstring(L,2);

    return 0;
}

int scripting::lua_httpreq_add_header(lua_State* L)
{
    ((httpreq*)luaL_checkudata(L,1,LUA_HTTP))->headers->push_back(luaL_checkstring(L,2));

    return 0;
}

int scripting::lua_httpreq_data(lua_State* L)
{
    httpreq* p=(httpreq*)luaL_checkudata(L,1,LUA_HTTP);

    lua_pushlstring(L,p->req->data.c_str(),p->req->data.length());

    return 1;
}

int scripting::lua_httpreq_data_length(lua_State* L)
{
    httpreq* p=(httpreq*)luaL_checkudata(L,1,LUA_HTTP);

    lua_pushinteger(L,p->req->data.length());

    return 1;
}

int scripting::lua_scan_for_media(lua_State* L)
{
    utils::do_scan_for_media();

    return 0;
}

int scripting::lua_browse(lua_State* L)
{
    int _contid=luaL_checkint(L,1);

    int page=luaL_checkint(L,2);

    int page_size=10;

    if(lua_gettop(L)>2)
        page_size=luaL_checkint(L,3);

    if(_contid<0)
        _contid=0;

    if(page<1)
        page=1;

    if(page_size<1)
        page_size=1;

    db::locker lock;

    std::string contid=utils::format("%d",_contid);

    int total=db::get_childs_count(contid);

    int rc=0;

    lua_pushinteger(L,total); rc++;

    if(total>0)
    {
        lua_newtable(L); rc++;

        std::map<std::string,std::string> row;

        db::rowset_t stmt;

        if(db::find_objects_by_parent_id(contid,page_size,(page-1)*page_size,stmt))
        {
            int idx=1;

            while(stmt.fetch(row))
            {

                lua_newtable(L);

                for(std::map<std::string,std::string>::const_iterator it=row.begin();it!=row.end();++it)
                {
                    const std::string& n=it->first;

                    const std::string& v=it->second;

                    lua_pushlstring(L,n.c_str(),n.length());
                    lua_pushlstring(L,v.c_str(),v.length());
                    lua_rawset(L,-3);
                }

                lua_rawseti(L,-2,idx++);
            }
        }
    }

    return rc;
}
