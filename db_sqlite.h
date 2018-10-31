/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __DB_SQLITE_H
#define __DB_SQLITE_H

#include <string>
#include <map>
#include "sqlite3.h"

namespace db_sqlite
{
    struct object_t
    {
        std::string     objid;
        std::string     parentid;
        int             objtype;
        std::string     items;
        std::string     handler;
        int             mimecode;
        std::string     length;
        std::string     name;
        std::string     url;
        std::string     logo;
        std::string     uuid;

        bool _is_empty;

        object_t(void):objtype(-1),mimecode(0),_is_empty(true) {}

        bool empty(void) { return _is_empty; }

        void clear(void)
        {
            objid.clear(); parentid.clear(); objtype=-1; items.clear();
            handler.clear(); mimecode=0; length.clear(); name.clear(); url.clear(); logo.clear(); uuid.clear();
            _is_empty=true;
        }
    };

    class stmt
    {
    public:
        sqlite3_stmt* st;

        object_t __row;

        stmt(void):st(NULL) {}

        ~stmt(void)
            { if(st) sqlite3_finalize(st); }

        bool prepare(const char* sql,...);

        bool bind(int idx,const std::string& value);

        bool bind_int64(int idx,u_int64_t value);

        bool bind_null(int idx);

        int column_count(void) { return sqlite3_column_count(st); }

        bool step(void)
            { return sqlite3_step(st)==SQLITE_ROW?true:false; }

        bool exec(void)
            { return sqlite3_step(st)==SQLITE_DONE?true:false; }

        bool fetch(std::map<std::string,std::string>& row);

        bool fetch(object_t& row);

        const object_t* fetch(void)
            { return fetch(__row)?&__row:NULL; }

        void text(int col,std::string& s)
        {
            const char* p=(const char*)sqlite3_column_text(st,col); int n=sqlite3_column_bytes(st,col);

            if(p && n>0)
                s.assign(p,n);
            else
                s.clear();
        }

        std::string text(int col)
        {
            std::string s;

            text(col,s);

            return s;
        }

        int integer(int col) { return sqlite3_column_int(st,col); }

        double number(int col) { return sqlite3_column_double(st,col); }
    };

    class locker
    {
    public:
        locker(void);
        ~locker(void);
    };

    void begin(void);

    bool exec(const char* sql,...);

    void end(void);

    // public API

    typedef stmt rowset_t;

    bool init(void);

    void done(void);

    bool find_object_by_id(const std::string& objid,object_t& obj);

    bool find_next_object_by_id(const std::string& objid,const std::string& parentid,object_t& obj);

    int get_childs_count(const std::string& parentid);

    bool find_objects_by_parent_id(const std::string& parentid,int limit,int offset,rowset_t& st);

    int get_objects_count(const std::string& from,const std::string& to,int what);

    bool search_objects(const std::string& from,const std::string& to,int what,int count,int offset,rowset_t& st);

    bool add_container(int contid,int parentid,int childs_num,const std::string& name);

    bool add_media(int objid,int parentid,int objtype,const std::string& handler,int mimecode,u_int64_t length,const std::string& name,
        const std::string& url,const std::string& logo,const std::string& uuid);

    void begin_scan(void);

    void end_scan(void);

    bool search_objects_without_meta(rowset_t& st);
}

#endif
