/*
 * Copyright (C) 2011-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "soap_int.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

namespace soap
{
    enum
    {
        cht_oth = 0,
        cht_al  = 1,
        cht_num = 2,
        cht_sp  = 4,
        cht_s   = 8
    };

    const char types[256]=
    {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x00,0x00,0x04,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x04,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
        0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x08,0x08,0x08,0x08,0x08,0x08,
        0x08,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
        0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x08,0x08,0x08,0x08,0x08,
        0x08,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
        0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x08,0x08,0x08,0x08,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    };

    void string::clear(void)
    {
        if(ptr)
        {
            free(ptr);
            ptr=0;
        }
        size=0;
    }

    void string::swap(string& s)
    {
        char* pp=s.ptr;
        int ss=s.size;
        s.ptr=ptr; s.size=size;
        ptr=pp; size=ss;
    }

    void string::trim_right(void)
    {
#ifndef NO_TRIM_DATA
        if(ptr && size>0)
        {
            for(;size>0 && types[ptr[size-1]]==cht_sp;size--);
            ptr[size]=0;
        }
#endif
    }

    int string_builder::add_chunk(void)
    {
        chunk* p=(chunk*)malloc(sizeof(chunk)+chunk::max_size);
        if(!p)
            return -1;

        p->ptr=(char*)(p+1);
        p->size=0;
        p->next=0;

        if(!beg)
            beg=end=p;
        else
        {
            end->next=p;
            end=p;
        }
        return 0;
    }

    void string_builder::clear(void)
    {
        while(beg)
        {
            chunk* tmp=beg;
            beg=beg->next;
            free(tmp);
        }
        beg=end=0;
    }

    void string_builder::add(const char* s,int len)
    {
        if(!s)
            return;
        if(len==-1)
            len=strlen(s);
        if(!len)
            return;

        while(len>0)
        {
            int n=end?(chunk::max_size-end->size):0;

            if(n<=0)
            {
                if(add_chunk())
                    return;
                n=chunk::max_size;
            }

            int m=len>n?n:len;
            memcpy(end->ptr+end->size,s,m);
            end->size+=m;
            len-=m;
            s+=m;
        }
    }

    void string_builder::swap(string& s)
    {
        int len=0;

        for(chunk* p=beg;p;p=p->next)
            len+=p->size;

        s.clear();

        if(len>0)
        {
            char* ptr=(char*)malloc(len+1);
            if(ptr)
            {
                char* pp=ptr;

                for(chunk* p=beg;p;p=p->next)
                {
                    memcpy(pp,p->ptr,p->size);
                    pp+=p->size;
                }

                *pp=0;

                s.ptr=ptr;
                s.size=len;
            }
        }

        clear();
    }

    node* node::add_node(void)
    {
        node* p=(node*)malloc(sizeof(node));
        if(!p)
            return 0;

        p->init();
        p->parent=this;

        if(!beg)
            beg=end=p;
        else
        {
            end->next=p;
            end=p;
        }

        return p;
    }

    void node::clear(void)
    {
        while(beg)
        {
            node* p=beg;

            beg=beg->next;

            p->clear();

            free(p);
        }

        beg=end=0;

        if(name) { free(name); name=0; }

        if(data) { free(data); data=0; len=0; }

        if(attr) { free(attr); attr=0; attr_len=0; }
    }

    node* node::find_child(const char* s,int l)
    {
        for(node* p=beg;p;p=p->next)
            if(p->name && !strncmp(p->name,s,l))
                return p;
        return 0;
    }

    node* node::find(const char* s)
    {
        node* pp=this;

        for(char *p1=(char*)s,*p2=0;p1 && pp;p1=p2)
        {
            int l=0;

            p2=strchr(p1,'/');
            if(p2)
                { l=p2-p1; p2++; }
            else
                l=strlen(p1);

            if(l)
                pp=pp->find_child(p1,l);
        }

        return pp;
    }

    void dump(node& node,int depth)
    {
        for(int i=0;i<depth;i++)
            printf("---");

        if(node.name)
        {
            if(node.len>0)
                printf("%s: attr=\"%s\", data=\"%s\"\n",node.name,node.attr?node.attr:"",node.data);
            else
                printf("%s: attr=\"%s\"\n",node.name,node.attr?node.attr:"");
        }

        for(soap::node* p=node.beg;p;p=p->next)
            dump(*p,depth+1);
    }
}

void soap::ctx::begin(void)
{
    st=0;
    err=0;
    line=0;
    tok_size=0;
    st_close_tag=0;
    st_quot=0;
    st_text=0;
}

int soap::ctx::end(void)
{
    if(tok_size>0 || st)
    {
        err=1;
        return -1;
    }

    data_push();

    return 0;
}

void soap::ctx::tok_push(void)
{
    tok[tok_size]=0;

    if(st==40)
    {
        if(tok_size)
            tag_open(tok,tok_size);
        tag_close("",0);
    }else
    {
        if(st_close_tag)
            tag_close(tok,tok_size);
        else
            tag_open(tok,tok_size);
    }

    tok_size=0;
    st_close_tag=0;
    st_quot=0;
    st=0;
}

void soap::ctx::tag_open(const char* s,int len)
{
    if(!len) { err=1; return; }

    node* p=cur->add_node();

    if(!p) { err=1; return; }

    p->name=(char*)malloc(len+1);

    if(p->name)
    {
        memcpy(p->name,s,len);
        p->name[len]=0;
    }
    cur=p;
}

void soap::ctx::tag_close(const char* s,int len)
{
    if(!cur->parent || !cur->name) { err=1; return; }

    if(len && strcmp(cur->name,s)) { err=1; return; }

    cur=cur->parent;
}

void soap::ctx::data_push(void)
{
    string s;
    data.swap(s);
    s.trim_right();

    if(s.length())
    {
        if(cur->data) free(cur->data);
        cur->data=s.ptr;
        cur->len=s.size;
        s.ptr=0;
        s.size=0;
    }
}

void soap::ctx::attr_push(void)
{
    if(!attributes)
        return;

    string s;
    data.swap(s);
    s.trim_right();

    if(s.size>0)
    {
        if(cur->attr) free(cur->attr);
        cur->attr=s.ptr;
        cur->attr_len=s.size;
        s.ptr=0;
        s.size=0;
    }
}

int soap::ctx::parse(const char* buf,int len)
{
    for(int i=0;i<len && !err;i++)
    {
        unsigned char ch=((unsigned char*)buf)[i];

        int cht=types[ch];

        if(ch=='\n')
            line++;

        switch(st)
        {
        case 0:
            if(ch=='<')
            {
                st=10;
                data_push();
                tok_reset();
                st_text=0;
            }else
            {
                switch(st_text)
                {
                case 0:
#ifndef NO_TRIM_DATA
                    if(cht==cht_sp) continue;
#endif
                    st_text=10;
                case 10:
                    if(ch=='&') { tok_add(ch); st_text=20; } else ch_push(ch);
                    break;
                case 20:
                    tok_add(ch);
                    if(ch==';')
                    {
                        tok[tok_size]=0;

                        if(!strcmp(tok,"&lt;")) ch_push('<');
                        else if(!strcmp(tok,"&gt;")) ch_push('>');
                        else if(!strcmp(tok,"&apos;")) ch_push('\'');
                        else if(!strcmp(tok,"&quot;")) ch_push('\"');
                        else if(!strcmp(tok,"&amp;")) ch_push('&');
                        tok_reset();
                        st_text=10;
                    }
                    break;
                }
            }
            break;
        case 10:
            if(ch=='/')
            {
                st=20;
                st_close_tag=1;
                continue;
            }else if(ch=='?')
            {
                st=50;
                continue;
            }else if(ch=='!')
            {
                st=60;
                continue;
            }
            st=11;
        case 11:
            if(cht!=cht_sp)
            {
                if(cht==cht_al)
                {
                    tok_add(ch);
                    st=20;
                }else
                    err=1;
            }
            break;
        case 20:
            if(cht==cht_sp) continue; st=21;
        case 21:
            if(ch==':')
                tok_reset();
            else if(cht==cht_sp)
            {
                tok_push();
                st=30;
            }else if(cht&(cht_al|cht_num) || ch=='_' || ch=='-')
                tok_add(ch);
            else if(ch=='/')
                st=40;
            else if(ch=='>')
                tok_push();
            else
                err=1;
            break;
        case 30:
            if(ch==' ')
                continue;
            st=31;
        case 31:
            if(st_quot)
            {
                if(ch=='\"')
                    st_quot=0;
            }else
            {
                if(ch=='\"')
                    st_quot=1;
                else if(ch=='/')
                    { st=40; attr_push(); }
                else if(ch=='>')
                    { st=0; attr_push(); }
            }

            if(attributes && st==31) ch_push(ch);

            break;
        case 40:
            if(ch!='>') err=1; else tok_push();
            break;
        case 50:
            if(ch=='?') st=55;
            break;
        case 55:
            if(ch=='>') st=0; else if(ch!='?') st=50;
            break;
        case 60:
            if(ch=='-') st=61; else if(ch=='[') st=100; else err=1;
            break;
        case 61:
            if(ch=='-') st=62; else err=1;
            break;
        case 62:
            if(ch=='-') st=63;
            break;
        case 63:
            if(ch=='-') st=64; else st=62;
            break;
        case 64:
            if(ch=='>')
                st=0;
            else if(ch!='-')
                st=62;
            break;
        case 100: if(ch=='C') st++; else err=1; break;
        case 101: if(ch=='D') st++; else err=1; break;
        case 102: if(ch=='A') st++; else err=1; break;
        case 103: if(ch=='T') st++; else err=1; break;
        case 104: if(ch=='A') st++; else err=1; break;
        case 105: if(ch=='[') st++; else err=1; break;
        case 106: if(ch==']') st=107; else ch_push(ch); break;
        case 107: if(ch==']') st=108; else { ch_push(']'); ch_push(ch); st=106; } break;
        case 108:
            if(ch=='>')
                { st=0; data_push(); }
            else if(ch==']')
                { ch_push(ch); }
            else
                { ch_push(']'); ch_push(']'); ch_push(ch); st=106; }
            break;
        }
    }

    return err;
}

int soap::parse(const std::string& s,node& root)
{
    soap::ctx ctx(&root);

    if(!ctx.parse(s.c_str(),s.length()) && !ctx.end())
        return 0;

    return -1;
}
