/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "db_mem.h"
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif /* _WIN32 */

namespace db_mem
{
    std::map<int,object_t> media_by_objid;

    void __attach_child(object_t& o,int parentid)
    {
        if(parentid>=0)
        {
            object_t& p=media_by_objid[parentid];
            o.parent=&p;

            if(!p.beg)
                p.beg=p.end=&o;
            else
            {
                p.end->next=&o;
                p.end=&o;
            }

        }
    }

    const object_t* __find_object_by_id(int objid)
    {
        std::map<int,object_t>::const_iterator it=media_by_objid.find(objid);

        if(it==media_by_objid.end())
            return NULL;

        return &it->second;
    }

    const object_t* __find_object_by_id(const std::string& objid)
    {
        if(objid.empty())
            return NULL;

        return __find_object_by_id(atoi(objid.c_str()));
    }

    const object_t* __skip(const object_t* o,int offset)
    {
        for(o=o->beg;o && offset>0;o=o->next,offset--);

        return o;
    }

    bool __search_objects(const std::string& from,const std::string& to,int what,int count,int offset,rowset_t& st);

#ifdef _WIN32
    CRITICAL_SECTION __db_lock;
#endif /*  _WIN32 */
}

bool db_mem::init(void)
{
#ifdef _WIN32
    InitializeCriticalSection(&__db_lock);
#endif /*  _WIN32 */

    return true;
}

void db_mem::done(void)
{
    media_by_objid.clear();

#ifdef _WIN32
    DeleteCriticalSection(&__db_lock);
#endif /*  _WIN32 */
}

void db_mem::begin_scan(void) { media_by_objid.clear(); }

void db_mem::end_scan(void)
{
    object_t* p=NULL;

    for(std::map<int,object_t>::iterator it=media_by_objid.begin();it!=media_by_objid.end();++it)
    {
        if(!p)
            p=&it->second;
        else
        {
            p->global_next=&it->second;
            p=&it->second;
        }
    }
}

db_mem::locker::locker(void)
{
#ifdef _WIN32
    EnterCriticalSection(&__db_lock);
#endif /*  _WIN32 */
}

db_mem::locker::~locker(void)
{
#ifdef _WIN32
    LeaveCriticalSection(&__db_lock);
#endif /*  _WIN32 */
}

bool db_mem::add_container(int contid,int parentid,int childs_num,const std::string& name)
{
    object_t& o=media_by_objid[contid];

    o.objid=utils::format("%d",contid);
    o.parentid=utils::format("%d",parentid);
    o.objtype=0;
    o.items=utils::format("%d",childs_num);
    o.name=name;

    __attach_child(o,parentid);

    return true;
}

bool db_mem::add_media(int objid,int parentid,int objtype,const std::string& handler,int mimecode,u_int64_t length,const std::string& name,
    const std::string& url,const std::string& logo,const std::string& uuid)
{
    object_t& o=media_by_objid[objid];

    o.objid=utils::format("%d",objid);
    o.parentid=utils::format("%d",parentid);
    o.objtype=objtype;
    o.handler=handler;
    o.mimecode=mimecode;
    if(length!=(u_int64_t)-1)
        o.length=utils::format("%llu",(unsigned long long int)length);
    o.name=name;
    o.url=url;
    o.logo=logo;

    __attach_child(o,parentid);

    return true;
}

bool db_mem::find_object_by_id(const std::string& objid,object_t& obj)
{
    const object_t* o=__find_object_by_id(objid);

    if(!o)
        return false;

    obj=*o;

    return true;
}

bool db_mem::find_next_object_by_id(const std::string& objid,const std::string& parentid,object_t& obj)
{
    const object_t* o=__find_object_by_id(objid);

    if(!o || !o->next)
        return false;

    obj=*o->next;

    return true;
}

int db_mem::get_childs_count(const std::string& parentid)
{
    const object_t* o=__find_object_by_id(parentid);

    if(!o)
        return false;

    return atoi(o->items.c_str());
}


bool db_mem::find_objects_by_parent_id(const std::string& parentid,int limit,int offset,rowset_t& st)
{
    const object_t* o=__find_object_by_id(parentid);

    if(!o || !o->beg)
        return false;

    st.cur=o->beg;

    st.limit=-1;

    while(offset>0 && st.__fetch())
        offset--;

    st.limit=limit>0?limit:-1;

    return true;
}

bool db_mem::__search_objects(const std::string& from,const std::string& to,int what,int count,int offset,rowset_t& st)
{
    const object_t* o1=__find_object_by_id(from);

    const object_t* o2;

    if(!o1)
        return false;

    if(!to.empty())
        o2=__find_object_by_id(atoi(to.c_str()));
    else
    {
//        std::map<int,object_t>::const_reverse_iterator it=media_by_objid.rbegin();
        std::map<int,object_t>::reverse_iterator it=media_by_objid.rbegin();

        if(it==media_by_objid.rend())
            return false;

        o2=&it->second;
    }

    st.cur=o1;

    st.last=o2;

    st.what=what;

    st._is_search=true;

    st.limit=-1;

    while(offset>0 && st.__fetch())
        offset--;

    if(!st.cur)
        return false;

    st.limit=count>0?count:-1;

    return true;
}

int db_mem::get_objects_count(const std::string& from,const std::string& to,int what)
{
    rowset_t st;

    if(!__search_objects(from,to,what,0,0,st))
        return 0;

    int num=0;

    while(st.__fetch())
        num++;

    return num;
}

bool db_mem::search_objects(const std::string& from,const std::string& to,int what,int count,int offset,rowset_t& st)
{
    return __search_objects(from,to,what,count,offset,st);
}

bool db_mem::rowset_t::fetch(std::map<std::string,std::string>& row)
{
    const object_t* o=__fetch();

    if(!o)
        return false;

    row["objid"]=o->objid;
    row["parentid"]=o->parentid;
    row["objtype"]=utils::format("%d",o->objtype);
    row["items"]=o->items;
    row["handler"]=o->handler;
    row["mimecode"]=utils::format("%d",o->mimecode);
    row["length"]=o->length;
    row["name"]=o->name;
    row["url"]=o->url;
    row["logo"]=o->logo;

    return true;
}

bool db_mem::rowset_t::fetch(object_t& row)
{
    const object_t* o=__fetch();

    if(!o)
        return false;

    row=*o;

    return true;
}

const db_mem::object_t* db_mem::rowset_t::__fetch(void)
{
    if(!cur || !limit)
        return NULL;

    if(_is_search)
        return __fetch_search();

    const object_t* o=cur;

    cur=cur->next;

    if(limit!=-1)
        limit--;

    return o;
}

const db_mem::object_t* db_mem::rowset_t::__fetch_search(void)
{
    for(;cur;cur=cur->global_next)
    {
        if(cur==last)
            { cur=NULL; break ; }

        if(!what)
        {
            if(cur->objtype==0)
                continue;
        }else
        {
            if(cur->objtype!=what)
                continue;
        }

        break;
    }

    const object_t* o=cur;

    if(!o)
        return NULL;

    cur=cur->global_next;

    if(limit!=-1)
        limit--;

    return o;
}

