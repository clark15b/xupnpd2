/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __DB_MEM_H
#define __DB_MEM_H

#include "common.h"
#include <string>
#include <map>

namespace db_mem
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

        object_t *parent,*next,*beg,*end,*global_next;

        object_t(void):objtype(-1),mimecode(0),parent(NULL),next(NULL),beg(NULL),end(NULL),global_next(NULL) {}

        bool empty(void) { return false; }

        void clear(void)
        {
            objid.clear(); parentid.clear(); objtype=-1; items.clear();
            handler.clear(); mimecode=0; length.clear(); name.clear(); url.clear(); logo.clear();
        }
    };

    class rowset_t
    {
    public:
        const object_t* cur;

        const object_t* last;

        int limit,what;

        bool _is_search;

        rowset_t(void):cur(NULL),last(NULL),limit(0),what(-1),_is_search(false) {}

        const object_t* __fetch(void);

        const object_t* __fetch_search(void);

        const object_t* fetch(void) { return __fetch(); }

        bool fetch(std::map<std::string,std::string>& row);

        bool fetch(object_t& row);
    };

    class locker
    {
    public:
        locker(void);
        ~locker(void);
    };

    bool init(void);

    void begin_scan(void);

    void end_scan(void);

    void done(void);

    bool add_container(int contid,int parentid,int childs_num,const std::string& name);

    bool add_media(int objid,int parentid,int objtype,const std::string& handler,int mimecode,u_int64_t length,const std::string& name,
        const std::string& url,const std::string& logo,const std::string& uuid);

    bool find_object_by_id(const std::string& objid,object_t& obj);

    bool find_next_object_by_id(const std::string& objid,const std::string& parentid,object_t& obj);

    int get_childs_count(const std::string& parentid);

    bool find_objects_by_parent_id(const std::string& parentid,int limit,int offset,rowset_t& st);

    int get_objects_count(const std::string& from,const std::string& to,int what);

    bool search_objects(const std::string& from,const std::string& to,int what,int count,int offset,rowset_t& st);
}

#endif
