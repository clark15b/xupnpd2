/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "compat.h"
#include "luajson.h"
#include "luacompat.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>

namespace libjson
{
    int no_unicode_escape=1;

    int lua_json_no_unicode_escape(lua_State* L);
    int lua_json_encode(lua_State* L);
    int lua_json_decode(lua_State* L);

    int lua_json_encode_value(lua_State* L,luaL_Buffer* buf,int index);

    int luaL_pack_utf8_to_unicode(luaL_Buffer* buf,const unsigned char* p,int len);
    int luaL_pack_unicode_to_utf8(luaL_Buffer* buf,const unsigned char* p);

    void luaL_addjsonlstring(luaL_Buffer* buf,const char* p,size_t l);
    void luaL_addjson_unesc_lstring(luaL_Buffer* buf,const unsigned char* p,size_t l);

    int lua_json_parse(lua_State* L,const unsigned char* p,size_t l);


    static const unsigned char utf8_tab[256]=
    {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x62,0x74,0x6e,0x00,0x66,0x72,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x5c,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
        0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
        0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
        0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
        0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,
        0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,0x81,
        0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
        0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x83,0x84,0x84,0x84,0x84,0x85,0x85,0x80,0x80
    };


    static const unsigned char unicode_tab[256]=
    {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2f,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x5c,0x00,0x00,0x00,
        0x00,0x00,0x08,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,0x00,
        0x00,0x00,0x0d,0x00,0x09,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    };

    static const unsigned char json_chtype[256]=
    {
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x00,0x00,0x01,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x03,0x04,0x04,0x00,
        0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
        0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x02,0x00,0x03,0x00,0x00,
        0x00,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
        0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x02,0x00,0x03,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    };

    static const char err_stack_overflow[]=             "JSON: stack overflow";
    static const char err_illegal_stack_state[]=        "JSON: illegal stack state";
    static const char err_invalid_literal_length[]=     "JSON: invalid literal length";
    static const char err_illegal_literal[]=            "JSON: illegal literal";
    static const char err_invalid_number_length[]=      "JSON: invalid number length";
    static const char err_illegal_number[]=             "JSON: illegal number";
    static const char err_illegal_character[]=          "JSON: illegal character '%c'";
    static const char err_unclosed_tag[]=               "JSON: unclosed tag";

    enum
    {
        cht_oth=0,
        cht_spc=1,
        cht_dm1=2,
        cht_dm2=3,
        cht_num=4,
        cht_lit=5
    };

    class json_st
    {
    public:
        enum {max_stack_depth=64};

        enum
        {
            st_value=0,
            st_object,
            st_object_key,
            st_object_value,
            st_object_value_2,
            st_array,
            st_array_value,
            st_string,
            st_number,
            st_literal,
            st_delim,
        };

    protected:
        lua_State* L;
        int st[max_stack_depth];
        int key[max_stack_depth];       // array key
        int index;

    public:
        json_st(lua_State* _L):index(0),L(_L)
        {
            for(int i=0;i<max_stack_depth;i++)
                { st[i]=st_value; key[i]=0; }
        }

        void push(int newst=st_value)
        {
            if(index>=max_stack_depth)
                luaL_error(L,err_stack_overflow);
            st[++index]=newst;
        }

        void pop(int n=1)
        {
            if(index<n)
                luaL_error(L,err_illegal_stack_state);
            index-=n;
        }

        void set(int newst) { st[index]=newst; }

        int get(int ind=0)
        {
            int n;

            if(ind>0 || (n=index+ind)<0)
                luaL_error(L,err_illegal_stack_state);

            return st[n];
        }

        int get_top(void) { return index; }

        void reset_key(void) { key[index]=0; }

        int get_key(void) { return ++key[index]; }
    };

    void luaL_addjson_unesc_lstring(luaL_Buffer* buf,const unsigned char* p,size_t l);

    class json_tok
    {
    protected:
        lua_State* L;

        const unsigned char* ptr;

    public:
        json_tok(lua_State* _L):L(_L),ptr(0) {}

        void join(const unsigned char* p) { ptr=p; }

        void push_string(const unsigned char* p)
        {
            if(ptr && p>=ptr)
            {
                luaL_Buffer buf;

                luaL_buffinit(L,&buf);

                luaL_addjson_unesc_lstring(&buf,ptr,p-ptr);

                luaL_pushresult(&buf);
            }

            ptr=NULL;
        }

        void push_literal(const unsigned char* p)
        {
            if(ptr && p>=ptr)
            {
                size_t len=p-ptr;

                char tmp[8];

                if(len>sizeof(tmp)-1)
                    luaL_error(L,err_invalid_literal_length);

                memcpy(tmp,ptr,len);

                tmp[len]=0;

                if(!strcmp(tmp,"null"))
                    lua_pushnil(L);
                else if(!strcmp(tmp,"true"))
                    lua_pushboolean(L,1);
                else if(!strcmp(tmp,"false"))
                    lua_pushboolean(L,0);
                else
                    luaL_error(L,err_illegal_literal);
            }

            ptr=NULL;
        }

        void push_number(const unsigned char* p)
        {
            if(ptr && p>=ptr)
            {
                char tmp[64];

                size_t len=p-ptr;

                if(len>=sizeof(tmp))
                    luaL_error(L,err_invalid_number_length);

                memcpy(tmp,ptr,len);

                tmp[len]=0;

                char* p=NULL;

                double n=strtod(tmp,&p);

                if(*p)
                    luaL_error(L,err_illegal_number);

                lua_pushnumber(L,n);
            }

            ptr=NULL;
        }
    };

    int lua_json_parse(lua_State* L,const unsigned char* p,size_t l);
}


int luaopen_luajson(lua_State* L)
{
    static const luaL_Reg lib[]=
    {
        { "no_unicode_escape",  libjson::lua_json_no_unicode_escape },
        { "encode",             libjson::lua_json_encode },
        { "decode",             libjson::lua_json_decode },
        { NULL,                 NULL }
    };

    luaL_register(L,"json",lib);

    return 0;
}

int libjson::lua_json_no_unicode_escape(lua_State* L)
{
    no_unicode_escape=luaL_checkinteger(L,1);

    return 0;
}

int libjson::luaL_pack_utf8_to_unicode(luaL_Buffer* buf,const unsigned char* p,int len)
{
    char tmp[32];
    int n=0;

    switch(len)
    {
    case 2:
        n=sprintf(tmp,"\\u%.4x",((((u_int16_t)p[0])<<6)&0x07c0)|(((u_int16_t)p[1])&0x003f));
        break;
    case 3:
        n=sprintf(tmp,"\\u%.4x",((((u_int16_t)p[0])<<12)&0xf000)|((((u_int16_t)p[1])<<6)&0x0fc0)|(((u_int16_t)p[2])&0x003f));    
        break;
    default:
        tmp[n++]='?';
        break;
    }

    luaL_addlstring(buf,tmp,n);

    return 0;
}

int libjson::luaL_pack_unicode_to_utf8(luaL_Buffer* buf,const unsigned char* p)
{
    unsigned int ch=0;

    char tmp[32];
    int n=0;

    {
        memcpy(tmp,p,4);
        tmp[4]=0;
        sscanf(tmp,"%x",&ch);
    }

    if(ch<=0x0000007F)
        n=sprintf(tmp,"%c",(char)(ch&0x7f));
    else if(ch>=0x00000080 && ch<=0x000007FF)
        n=sprintf(tmp,"%c%c",(char)(((ch>>6)&0x1f)|0xc0),(char)((ch&0x3f)|0x80));
    else if(ch>=0x00000800 && ch<=0x0000FFFF)
        n=sprintf(tmp,"%c%c%c",(char)(((ch>>12)&0x0f)|0xe0),(char)(((ch>>6)&0x3f)|0x80),(char)((ch&0x3f)|0x80));
    else
        tmp[n++]='?';

    luaL_addlstring(buf,tmp,n);

    return 0;
}

void libjson::luaL_addjsonlstring(luaL_Buffer* buf,const char* p,size_t l)
{
    for(size_t i=0;i<l;++i)
    {
        unsigned char ch=p[i];

        unsigned char type=utf8_tab[ch];

        if(!type)
            luaL_addchar(buf,ch);
        else
        {
            if(type&0x80)
            {
                if(no_unicode_escape)
                {
                    luaL_addchar(buf,ch);
                }else
                {
                    int n=type&0x0f;

                    if(!n)
                        break;
                    else
                    {
                        if(l-i>n)
                        {
                            if(luaL_pack_utf8_to_unicode(buf,(unsigned char*)(p+i),n+1))
                                break;

                            i+=n;
                        }else
                            break;
                    }
                }
            }else
            {
                luaL_addchar(buf,'\\');

                luaL_addchar(buf,type);
            }
        }
    }
}

void libjson::luaL_addjson_unesc_lstring(luaL_Buffer* buf,const unsigned char* p,size_t l)
{
    for(size_t i=0;i<l;++i)
    {
        unsigned char ch=p[i];

        if(ch=='\\')
        {
            unsigned char type=unicode_tab[p[++i]];

            if(type==0xff)
            {
                if(l-i>3)
                {
                    if(luaL_pack_unicode_to_utf8(buf,p+i+1))
                        break;

                    i+=4;
                }
            }else if(type!=0)
                luaL_addchar(buf,type);
        }else
            luaL_addchar(buf,ch);
    }
}

int libjson::lua_json_encode_value(lua_State* L,luaL_Buffer* buf,int index)
{
    static const char l_null[]="null";
    static const char l_true[]="true";
    static const char l_false[]="false";

    size_t l;
    const char* p;

    int type=lua_type(L,index);

    switch(type)
    {
    case LUA_TNUMBER:
        p=lua_tolstring(L,index,&l);

        if(p)
            luaL_addlstring(buf,p,l);
        else
            luaL_addlstring(buf,l_null,sizeof(l_null)-1);

        break;
    case LUA_TBOOLEAN:
        if(lua_toboolean(L,index)>0)
            luaL_addlstring(buf,l_true,sizeof(l_true)-1);
        else
            luaL_addlstring(buf,l_false,sizeof(l_false)-1);

        break;
    case LUA_TSTRING:
        luaL_addchar(buf,'\"');

        p=lua_tolstring(L,index,&l);

        if(p)
            luaL_addjsonlstring(buf,p,l);

        luaL_addchar(buf,'\"');

        break;
    case LUA_TTABLE:
        {
            int ind=index>0?index:lua_gettop(L)+index+1;

            lua_pushnil(L);

            int is_array=0;

            int i=0;

            while(lua_next(L,ind))
            {
                if(!i)
                {
                    if(lua_type(L,-2)==LUA_TNUMBER)
                    {
                        is_array++;

                        luaL_addchar(buf,'[');
                    }else
                        luaL_addchar(buf,'{');
                }

                if(is_array)
                {
                    if(i)
                        luaL_addchar(buf,',');

                    lua_json_encode_value(L,buf,-1);
                }else
                {
                    if(lua_type(L,-2)==LUA_TSTRING)
                    {
                        if(i)
                            luaL_addchar(buf,',');

                        luaL_addchar(buf,'\"');

                        size_t l=0;

                        const char* p=lua_tolstring(L,-2,&l);

                        luaL_addlstring(buf,p,l);

                        luaL_addchar(buf,'\"');

                        luaL_addchar(buf,':');

                        lua_json_encode_value(L,buf,-1);
                    }
                }

                lua_pop(L,1);

                i++;
            }

            if(i>0)
            {
                if(is_array)
                    luaL_addchar(buf,']');
                else
                    luaL_addchar(buf,'}');
            }else
                luaL_addlstring(buf,l_null,sizeof(l_null)-1);
        }

        break;
    default:
        luaL_addlstring(buf,l_null,sizeof(l_null)-1);

        break;
    }

    return 0;
}

int libjson::lua_json_encode(lua_State* L)
{
    int n=lua_gettop(L);

    luaL_Buffer buf;

    luaL_buffinit(L,&buf);

    for(int i=1;i<=n;++i)
    {
        if(i>1)
            luaL_addchar(&buf,',');

        lua_json_encode_value(L,&buf,i);
    }

    luaL_pushresult(&buf);

    return 1;
}

int libjson::lua_json_decode(lua_State* L)
{
    size_t l=0;

    const unsigned char* p=(unsigned char*)luaL_checklstring(L,1,&l);

    return lua_json_parse(L,p,l);
}

int libjson::lua_json_parse(lua_State* L,const unsigned char* p,size_t l)
{
    int top=lua_gettop(L);

    json_st st(L);

    json_tok tok(L);

    for(size_t i=0;i<=l;++i)
    {
        unsigned char ch=p[i];

        int chtype=json_chtype[ch];

        switch(st.get())
        {
        case json_st::st_value:
            if(chtype==cht_spc)
                break;

            if(ch=='[') { lua_newtable(L); st.reset_key(); if(st.get_top()>0) st.set(json_st::st_array); else st.push(json_st::st_array); }
            else if(ch=='{') { lua_newtable(L); if(st.get_top()>0) st.set(json_st::st_object); else st.push(json_st::st_object); }
            else if(ch=='\"') { tok.join(p+i+1); if(st.get_top()>0) st.set(json_st::st_string); else st.push(json_st::st_string); }
            else if(chtype==cht_num) { tok.join(p+i); if(st.get_top()>0) st.set(json_st::st_number); else st.push(json_st::st_number); }
            else if(chtype==cht_lit) { tok.join(p+i); if(st.get_top()>0) st.set(json_st::st_literal); else st.push(json_st::st_literal); }
            else luaL_error(L,err_illegal_character,ch);

            break;
        case json_st::st_string:
            if(ch=='\\') { i++; break; }
            else if(ch!='\"') break;

            tok.push_string(p+i);

            if(st.get_top()>1) st.pop(); else st.set(json_st::st_delim);

            break;
        case json_st::st_number:
            if(chtype!=cht_spc && chtype!=cht_dm2)
                break;

            tok.push_number(p+i);

            if(st.get_top()>1) st.pop(); else st.set(json_st::st_delim);

            i--;

            break;
        case json_st::st_literal:
            if(chtype!=cht_spc && chtype!=cht_dm2)
                break;

            tok.push_literal(p+i);

            if(st.get_top()>1) st.pop(); else st.set(json_st::st_delim);

            i--;

            break;
        case json_st::st_delim:

            if(ch==',') st.pop();
            else if(chtype==cht_dm2 || ch=='\0') { st.pop(); i--; }
            else if(chtype!=cht_spc)
                luaL_error(L,err_illegal_character,ch);

            break;
        case json_st::st_object:
            if(chtype==cht_spc)
                break;

            switch(ch)
            {
            case '\"':
                tok.join(p+i+1);
                st.set(json_st::st_object_key);
                st.push(json_st::st_string);
                break;
            case '}':
                if(st.get_top()>1) st.pop(); else st.set(json_st::st_delim);
                break;
            default:
                luaL_error(L,err_illegal_character,ch);
                break;
            }

            break;
        case json_st::st_object_key:
            if(chtype==cht_spc)
                break;

            if(ch==':')
                st.set(json_st::st_object_value);
            else
                luaL_error(L,err_illegal_character,ch);

            break;
        case json_st::st_object_value:
            if(chtype==cht_spc)
                break;

            if(chtype!=cht_dm1 && chtype!=cht_num && chtype!=cht_lit && ch!='\"')
                luaL_error(L,err_illegal_character,ch);

            st.set(json_st::st_object_value_2);
            st.push(json_st::st_value);
            i--;

            break;
        case json_st::st_object_value_2:
            if(chtype==cht_spc)
                break;

            switch(ch)
            {
            case '}':
                lua_rawset(L,-3);
                if(st.get_top()>1) st.pop(); else st.set(json_st::st_delim);
                break;
            case ',':
                st.set(json_st::st_object);
                lua_rawset(L,-3);
                break;
            default:
                luaL_error(L,err_illegal_character,ch);
                break;
            }

            break;
        case json_st::st_array:
            if(chtype==cht_spc)
                break;

            if(ch==']')
                if(st.get_top()>1) st.pop(); else st.set(json_st::st_delim);
            else if(chtype!=cht_dm1 && chtype!=cht_num && chtype!=cht_lit && ch!='\"')
                luaL_error(L,err_illegal_character,ch);
            else
            {
                st.set(json_st::st_array_value);
                st.push(json_st::st_value);
                i--;
            }

            break;
        case json_st::st_array_value:
            if(chtype==cht_spc)
                break;

            switch(ch)
            {
            case ']':
                lua_rawseti(L,-2,st.get_key());
                if(st.get_top()>1) st.pop(); else st.set(json_st::st_delim);
                break;
            case ',':
                lua_rawseti(L,-2,st.get_key());
                st.set(json_st::st_array);
                break;
            default:
                luaL_error(L,err_illegal_character,ch);
                break;
            }

            break;
        }
    }

    int s=st.get();

    if(s!=json_st::st_value)
        luaL_error(L,err_unclosed_tag);

    return lua_gettop(L)-top;
}
