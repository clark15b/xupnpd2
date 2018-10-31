/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "common.h"
#include "db_sqlite.h"
#include <stdarg.h>
#include <stdio.h>

namespace db_sqlite
{
    static const char __fields[]="objid,parentid,objtype,items,handler,mimecode,length,name,url,logo,uuid";

    void __fill_fields(db_sqlite::stmt& s,object_t& o)
    {
        s.text(0,o.objid);
        s.text(1,o.parentid);
        o.objtype=s.integer(2);
        s.text(3,o.items);
        s.text(4,o.handler);
        o.mimecode=s.integer(5);
        s.text(6,o.length);
        s.text(7,o.name);
        s.text(8,o.url);
        s.text(9,o.logo);
        s.text(10,o.uuid);

        o._is_empty=false;
    }

    sqlite3* db=NULL;

    bool __search_objects_exp(const std::string& from,const std::string& to,int what,std::string& exp);
}

bool db_sqlite::init(void)
{
    int rc=sqlite3_open(cfg::db_file.c_str(),&db);

    if(rc)
        { done(); utils::trace(utils::log_err,"SQL: unable connect to: %s",cfg::db_file.c_str()); return false; }

    exec(
        "CREATE TABLE IF NOT EXISTS MEDIA ("
        "objid INTEGER PRIMARY KEY,"
        "parentid INTEGER,"
        "objtype INTEGER,"
        "items INTEGER,"
        "handler VARCHAR,"
        "mimecode INTEGER,"
        "length INTEGER,"
        "name VARCHAR,"
        "url VARCHAR,"
        "logo VARCHAR,"
        "uuid VARCHAR"
        ");"
        "CREATE INDEX IF NOT EXISTS MEDIA_PARENT ON MEDIA(parentid);"
        "CREATE INDEX IF NOT EXISTS MEDIA_NAME ON MEDIA(name);"
        "CREATE INDEX IF NOT EXISTS MEDIA_NAME ON MEDIA(uuid);"
        );

    exec(
        "CREATE TABLE IF NOT EXISTS META ("
        "uuid VARCHAR PRIMARY KEY,"
        "title VARCHAR,"
        "episode VARCHAR,"
        "poster VARCHAR,"
        "imdb VARCHAR"
        ");"
        );

    return true;
}

void db_sqlite::done(void)
{
    if(db)
        { sqlite3_close(db); db=NULL; }
}

db_sqlite::locker::locker(void)
{
}

db_sqlite::locker::~locker(void)
{
}

void db_sqlite::begin(void)
    { exec("BEGIN TRANSACTION"); }

void db_sqlite::end(void)
    { exec("END TRANSACTION"); }

bool db_sqlite::stmt::prepare(const char* sql,...)
{
    char s[1024];

    va_list ap;
    va_start(ap,sql);

    int n=vsnprintf(s,sizeof(s),sql,ap);

    va_end(ap);

    if(n==-1 || n>=sizeof(s))
        { utils::trace(utils::log_err,"SQL: buffer is too small"); return false; }

    int rc=sqlite3_prepare_v2(db,s,n,&st,NULL);

    if(rc!=SQLITE_OK)
        { utils::trace(utils::log_err,"SQL: %s [%s]",sqlite3_errstr(rc),s); return false; }

    return true;
}

bool db_sqlite::stmt::bind(int idx,const std::string& value)
{
    if(sqlite3_bind_text(st,idx,value.c_str(),value.length(),SQLITE_STATIC)==SQLITE_OK)
        return true;

    return false;
}

bool db_sqlite::stmt::bind_int64(int idx,u_int64_t value)
{
    if(sqlite3_bind_int64(st,idx,value)==SQLITE_OK)
        return true;

    return false;
}

bool db_sqlite::stmt::bind_null(int idx)
{
    if(sqlite3_bind_null(st,idx)==SQLITE_OK)
        return true;

    return false;
}

bool db_sqlite::stmt::fetch(std::map<std::string,std::string>& row)
{
    row.clear();

    if(!step())
        return false;

    for(int i=0;i<sqlite3_column_count(st);i++)
    {
        const char* name=sqlite3_column_name(st,i);

        const char* value=(const char*)sqlite3_column_text(st,i);

        int nvalue=sqlite3_column_bytes(st,i);

        if(name && value && nvalue>0)
            row[name].assign(value,nvalue);
    }

    return true;
}

bool db_sqlite::stmt::fetch(object_t& row)
{
    row.clear();

    if(!step())
        return false;

    __fill_fields(*this,row);

    return true;
}

bool db_sqlite::exec(const char* sql,...)
{
    char stmt[1024];

    va_list ap;
    va_start(ap,sql);

    int n=vsnprintf(stmt,sizeof(stmt),sql,ap);

    va_end(ap);

    if(n==-1 || n>=sizeof(stmt))
        { utils::trace(utils::log_err,"SQL: buffer is too small"); return false; }

    char* err=NULL;

    int rc=::sqlite3_exec(db,stmt,NULL,NULL,&err);

    if(rc!=SQLITE_OK)
    {
        std::string s(err); sqlite3_free(err);

        utils::trace(utils::log_err,"SQL: %s [%s]",s.c_str(),stmt);

        return false;
    }

//    sqlite3_int64 sqlite3_last_insert_rowid(sqlite3*);

    return true;
}

bool db_sqlite::find_object_by_id(const std::string& objid,object_t& obj)
{
    db_sqlite::stmt stmt;

    bool rc;

    if(objid.length()==32)
        rc=stmt.prepare("SELECT %s FROM MEDIA WHERE uuid='%s' LIMIT 1",__fields,objid.c_str());
    else
        rc=stmt.prepare("SELECT %s FROM MEDIA WHERE objid='%s' LIMIT 1",__fields,objid.c_str());

    if(!rc || !stmt.step())
        return false;

    __fill_fields(stmt,obj);

    return true;
}

bool db_sqlite::find_next_object_by_id(const std::string& objid,const std::string& parentid,object_t& obj)
{
    db_sqlite::stmt stmt;

    if(!stmt.prepare("SELECT %s FROM MEDIA WHERE objid>%s AND parentid<=%s ORDER BY objid LIMIT 1",
        __fields,objid.c_str(),parentid.c_str()) || !stmt.step())
            return false;

    __fill_fields(stmt,obj);

    return true;
}

int db_sqlite::get_childs_count(const std::string& parentid)
{
    db_sqlite::stmt stmt;

    if(stmt.prepare("SELECT count(*) FROM MEDIA WHERE parentid=%s",parentid.c_str()) && stmt.step())
        return stmt.integer(0);

    return 0;
}

bool db_sqlite::find_objects_by_parent_id(const std::string& parentid,int limit,int offset,rowset_t& st)
{
    return st.prepare("SELECT %s FROM MEDIA WHERE parentid=%s ORDER BY name LIMIT %d OFFSET %d",__fields,parentid.c_str(),limit,offset);
}

bool db_sqlite::__search_objects_exp(const std::string& from,const std::string& to,int what,std::string& exp)
{
    if(!from.empty())
    {
        exp+=utils::format("objid>%s",from.c_str());

        if(!to.empty())
            exp+=utils::format(" AND objid<%s",to.c_str());

        if(what!=0)
            exp+=utils::format(" AND objtype=%d",what);
        else
            exp+=utils::format(" AND objtype<>0",what);

        return true;
    }

    return false;
}

int db_sqlite::get_objects_count(const std::string& from,const std::string& to,int what)
{
    std::string exp;

    if(!__search_objects_exp(from,to,what,exp))
        return 0;

    db_sqlite::stmt stmt;

    if(!stmt.prepare("SELECT count(*) FROM MEDIA WHERE %s ORDER BY objid",exp.c_str()) || !stmt.step())
        return 0;

    return stmt.integer(0);
}

bool db_sqlite::search_objects(const std::string& from,const std::string& to,int what,int count,int offset,rowset_t& st)
{
    std::string exp;

    if(!__search_objects_exp(from,to,what,exp))
        return false;

    if(!st.prepare("SELECT %s FROM MEDIA WHERE %s ORDER BY objid LIMIT %d OFFSET %d",__fields,exp.c_str(),count,offset))
        return false;

    return true;
}

bool db_sqlite::add_media(int objid,int parentid,int objtype,const std::string& handler,int mimecode,u_int64_t length,const std::string& name,
    const std::string& url,const std::string& logo,const std::string& uuid)
{
    db_sqlite::stmt stmt;

    if(stmt.prepare(
        "INSERT INTO MEDIA(objid,parentid,objtype,handler,mimecode,length,name,url,logo,uuid) "
        "VALUES(%d,%d,%d,?,%d,?,?,?,?,?)",
        objid,parentid,objtype,mimecode) &&
            (handler.empty()?stmt.bind_null(1):stmt.bind(1,handler)) &&
            (length==(u_int64_t)-1?stmt.bind_null(2):stmt.bind_int64(2,length)) &&
            stmt.bind(3,name) &&
            stmt.bind(4,url) &&
            (logo.empty()?stmt.bind_null(5):stmt.bind(5,logo)) &&
            (uuid.empty()?stmt.bind_null(6):stmt.bind(6,uuid)) &&
            stmt.exec())
                return true;

    return false;
}

bool db_sqlite::add_container(int contid,int parentid,int childs_num,const std::string& name)
{
    db_sqlite::stmt stmt;

    if(stmt.prepare("INSERT INTO MEDIA(objid,parentid,objtype,items,name) VALUES(%d,%d,0,%d,?)",contid,parentid,childs_num) &&
        stmt.bind(1,name) && stmt.exec())
            return true;

    return false;
}

void db_sqlite::begin_scan(void)
{
    exec("PRAGMA synchronous=OFF; PRAGMA journal_mode=MEMORY;");

    begin();

    exec("DELETE FROM MEDIA");
}

void db_sqlite::end_scan(void)
{
    end();

    exec("PRAGMA synchronous=FULL; PRAGMA journal_mode=DELETE;");
}

bool db_sqlite::search_objects_without_meta(rowset_t& st)
{
    if(!st.prepare("SELECT m.name,m.uuid FROM MEDIA m left join META t ON m.uuid=t.uuid WHERE m.uuid IS NOT NULL AND m.objtype=1 AND t.uuid IS NULL"))
        return false;

    return true;
}
