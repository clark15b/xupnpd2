/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "scan.h"
#include "common.h"
#include "db.h"
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <map>
#include "mime.h"
#include <ctype.h>
#include "charset.h"

#ifndef _WIN32
#include <dirent.h>
#else
#include <io.h>
#endif /* _WIN32 */

namespace utils
{
    using namespace db;

    class finddata
    {
    protected:
        const std::string* path;

#ifndef _WIN32
        dirent* de; DIR* h;
#else
        __finddata64_t de; intptr_t h;
#endif
    public:
#ifndef _WIN32
        finddata(void):h(NULL) {}

        ~finddata(void)
            { if(h) closedir(h); }
#else
        finddata(void):h(-1) {}

        ~finddata(void)
            { if(h!=-1) _findclose(h); }
#endif
        std::string fullname;

        bool findfirst(const std::string& s)
        {
#ifndef _WIN32
            if(!(h=opendir(s.c_str())) || !(de=readdir(h)))
                return false;
#else
            h=_findfirsti64((s+"/*.*").c_str(),&de);

            if(h==-1)
                return false;
#endif
            path=&s;

            fullname=(*path)+OS_SLASH+name();

            return true;
        }

        bool findnext(void)
        {
#ifndef _WIN32
            if(!(de=readdir(h)))
                return false;
#else
            if(_findnexti64(h,&de))
                return false;
#endif
            fullname=(*path)+OS_SLASH+name();

            return true;
        }

#ifndef _WIN32
        const char* name(void) { return de->d_name; }

        bool is_directory()
        {
            DIR* hh=opendir(((*path)+OS_SLASH+de->d_name).c_str());

            if(hh)
                { closedir(hh); return true; }

            return false;
        }
#else
        const char* name(void) { return de.name; }

        bool is_directory() { return de.attrib&_A_SUBDIR?true:false; }
#endif

        u_int64_t length(void)
        {
#ifndef _WIN32
            return 0;
#else
            return de.size;
#endif
        }
    };

    int scan_directory(const std::string& path,int parentid,int& objid);

    int scan_playlist(const std::string& path,int parentid,int& objid,std::string& name);

    void m3u_parse_track_ext(const std::string& track_ext,std::map<std::string,std::string>& dst);

    bool __is_number(const std::string& s,int len);
}

void utils::m3u_parse_track_ext(const std::string& track_ext,std::map<std::string,std::string>& dst)
{
    const char* name=0; int nname=0;
    const char* value=0; int nvalue=0;

    int st=0;

    for(const char* p=track_ext.c_str();;p++)
    {
        switch(st)
        {
        case 0:
            if(*p!=' ')
                { name=p; st=1; }
            break;
        case 1:
            if(*p=='=')
                { nname=p-name; st=2; }
            else if(*p==' ') st=0;

            break;
        case 2:
            if(*p=='\"')
                st=10;
            else
                { value=p; st=3; }

            break;
        case 3:
            if(*p==' ' || *p==0)
            {
                nvalue=p-value;

                if(nname>0 && nvalue>0)
                    dst[std::string(name,nname)]=std::string(value,nvalue);

                nname=0; nvalue=0; st=0;
            }
            break;
        case 10:
            if(*p=='\"')
                { nname=0; st=0; }
            else
                { value=p; st=11; }
            break;
        case 11:
            if(*p=='\"')
            {
                nvalue=p-value;

                if(nname>0 && nvalue>0)
                    dst[std::string(name,nname)]=std::string(value,nvalue);

                nname=0; nvalue=0; st=0;
            }
            break;
        }

        if(!*p)
            break;
    }
}

int utils::scan_playlist(const std::string& path,int parentid,int& objid,std::string& name)
{
    int count=0;

    std::string playlist_name,ext;
    std::map<std::string,std::string> pls_ext;

    std::string::size_type n=path.find_last_of("\\/");

    if(n!=std::string::npos)
        playlist_name=path.substr(n+1);
    else
        playlist_name=path;

    n=playlist_name.find_last_of('.');

    if(n!=std::string::npos)
        { ext=playlist_name.substr(n+1); playlist_name=playlist_name.substr(0,n); }

    if(ext!="m3u" && ext!="m3u8")
        return 0;

    FILE* fp=fopen(path.c_str(),"rb");

    if(!fp)
        return 0;
    else
    {
        std::string track_name,_track_ext,track_url,track_type,track_handler,track_filter;

        std::map<std::string,std::string> track_ext;

        char buf[512];

        bool is_first=true;

        while(fgets(buf,sizeof(buf),fp))
        {
            char* beg=buf;

            while(*beg==' ' || *beg=='\t')
                beg++;

            char* p=strpbrk(beg,"\r\n");
            if(p)
                *p=0;
            else
                p=beg+strlen(beg);

            while(p>beg && (p[-1]==' ' || p[-1]=='\t'))
                p--;
            *p=0;

            p=beg;

            if(is_first)
            {
                if(!strncmp(p,"\xEF\xBB\xBF",3))    // skip BOM
                    p+=3;

                if(strncmp(p,"#EXTM3U",7))
                    return 0;

                is_first=false;
            }

            if(!*p)
                continue;

            if(*p=='#')
            {
                p++;

                static const char tag[]="EXTINF:";
                static const char tag_pls[]="EXT-X-M3U:";

                if(!strncmp(p,tag,sizeof(tag)-1))
                {
                    p+=sizeof(tag)-1;
                    while(*p==' ')
                        p++;

                    char* p2=strchr(p,',');
                    if(p2)
                    {
                        *p2=0; p2++;

                        while(*p2==' ') p2++;

                        char* p3=strchr(p,' ');
                        if(p3)
                        {
                            p3++;
                            while(*p3==' ')
                                p3++;

                            _track_ext=p3;
                        }

                        track_name=p2;
                    }
                }else if(!strncmp(p,tag_pls,sizeof(tag_pls)-1))
                {
                    p+=sizeof(tag_pls)-1;

                    while(*p && *p==' ')
                        p++;

                    m3u_parse_track_ext(p,pls_ext);
                }
            }else
            {
                track_url=p;

                std::map<std::string,std::string> pls_extra;

                m3u_parse_track_ext(_track_ext,track_ext);

                track_type=track_ext["type"];

                if(track_type.empty())
                    track_type=pls_ext["type"];

                track_handler=track_ext["handler"];

                if(track_handler.empty())
                    track_handler=pls_ext["handler"];

                track_filter=track_ext["filter"];

                if(track_filter.empty())
                    track_filter=pls_ext["filter"];

                if(track_type.empty())
                {
                    std::string::size_type n=track_url.find_last_of("\\/");

                    if(n!=std::string::npos)
                    {
                        n++;

                        std::string::size_type n1=track_url.find('?',n);

                        std::string filename;

                        if(n1==std::string::npos)
                            filename=track_url.substr(n);
                        else
                            filename=track_url.substr(n,n1-n);

                        n=filename.find_last_of('.');

                        if(n!=std::string::npos)
                            track_type=filename.substr(n+1);
                    }
                }

                if(track_handler.empty())
                {
                    if(!strncmp(track_url.c_str(),"http://",7))
                    {
                        if(!strncmp(track_type.c_str(),"m3u",3))
                            track_handler="hls";
                        else
                            track_handler="http";
                    }else if(!strncmp(track_url.c_str(),"udp://",6) || !strncmp(track_url.c_str(),"rtp://",6))
                        track_handler="udp";
                }

                if(track_handler=="hls" || track_handler=="udp")
                    track_type=cfg::upnp_live_type;
                else if(track_handler=="http" && track_type.empty())
                    track_type=cfg::upnp_http_type;

                if(!track_filter.empty() && !track_handler.empty())
                    track_handler=track_handler+'/'+track_filter;

                mime::type_t* t=mime::get_by_name(track_type);

                if(t && t->upnp_proto)
                {
                    ++objid; ++count;

//                    utils::trace(utils::log_debug,"- %s [%d/%d]",track_url.c_str(),parentid,objid);

                    add_media(objid,parentid,t->type,track_handler,t->id,(u_int64_t)-1,track_name,track_url,track_ext["logo"],std::string());
                }

                track_name.clear();
                _track_ext.clear();
                track_ext.clear();
                track_url.clear();
                track_type.clear();
                track_handler.clear();
            }
        }

        fclose(fp);
    }

    {
        std::map<std::string,std::string>::const_iterator it=pls_ext.find("name");

        if(it!=pls_ext.end() && !it->second.empty())
            name=it->second;
    }

    return count;
}

int utils::scan_directory(const std::string& path,int parentid,int& objid)
{
    int count=0;

    finddata de;

    if(de.findfirst(path))
    {
        do
        {
            if(*de.name()!='.')
            {
                if(de.is_directory())
                {
                    ++objid; ++count;

//                    utils::trace(utils::log_debug,"+ %s [%d/%d]",de.fullname.c_str(),parentid,objid);

                    int contid=objid;

                    int num=scan_directory(de.fullname,contid,objid);

                    add_container(contid,parentid,num,charset::to_utf8(de.name()));
                }else
                {
                    const char* p=strrchr(de.name(),'.');

                    if(p)
                    {
                        if(!strncmp(p+1,"m3u",3))
                        {
                            ++objid; ++count;

//                            utils::trace(utils::log_debug,"* %s [%d/%d]",de.fullname.c_str(),parentid,objid);

                            int contid=objid;

                            std::string name=charset::to_utf8(std::string(de.name(),p-de.name()));

                            int num=scan_playlist(de.fullname,contid,objid,name);

                            add_container(contid,parentid,num,name);
                        }else
                        {
                            mime::type_t* t=mime::get_by_name(p+1);

                            if(t && t->upnp_proto)
                            {
                                u_int64_t length=de.length();

                                if(!length)
                                    length=utils::get_file_length(de.fullname);

                                ++objid; ++count;

//                                utils::trace(utils::log_debug,"- %s [%d/%d]",de.fullname.c_str(),parentid,objid);

                                std::string unique_id;

                                if(t->type==mime::video || t->type==mime::audio)
                                    utils::md5(de.fullname,unique_id);

                                add_media(objid,parentid,t->type,std::string(),t->id,length,charset::to_utf8(std::string(de.name(),p-de.name())),
                                        de.fullname,std::string(),unique_id);
                            }
                        }
                    }
                }
            }
        }while(de.findnext());
    }


    return count;
}

int utils::scan_for_media(void)
{
    utils::trace(utils::log_info,"scanning '%s'...",cfg::media_root.c_str());

    db::locker lock;

    db::begin_scan();

    cfg::system_update_id++;

    int objid=cfg::upnp_objid_offset;

    int num=scan_directory(cfg::media_root,0,objid);

    add_container(0,-1,num,cfg::upnp_device_name);

//update_media_meta();

    db::end_scan();

    int n=objid-cfg::upnp_objid_offset;

    utils::trace(utils::log_info,"scan complete, total: %i",n);

    return n;
}

int utils::update_media_meta(void)
{
#ifdef NO_SQLITE
    return 0;
#else
    db::rowset_t st;

    if(db::search_objects_without_meta(st))
    {
        while(st.step())
        {
            meta_data meta;
            get_meta_data_by_filename(st.text(0),meta);
//            printf("%s, %s\n",st.text(0).c_str(),meta.title.c_str());

            db::exec("insert into META(uuid,title) VALUES('%s','%s')",st.text(1).c_str(),meta.title.c_str());
        }
    }

    return 0;
#endif /* NO_SQLITE */
}

bool utils::__is_number(const std::string& s,int len)
{
    if(len!=-1 && s.length()!=len)
        return false;

    for(const char* p=s.c_str();*p;p++)
        if(!isdigit(*p))
            return false;

    return true;
}

bool utils::get_meta_data_by_filename(const std::string& filename,meta_data& meta)
{
    enum { padding=0, digit=1, alpha_hi=2, alpha_lo=3 };

    static const char t[256]=
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    std::map<int,std::string> parts;

    int idx=0;

    {
        std::string::size_type maxn=filename.find_last_of('.');

        if(maxn==std::string::npos)
            maxn=filename.length();

        std::string s;

        int last_cht=-1;

        const unsigned char* p=(const unsigned char*)filename.c_str();

        for(int i=0;i<maxn;i++)
        {
            int ch=*((const unsigned char*)p); p++; int cht=t[ch];

            if(last_cht!=-1 && (
                cht==padding ||
                (last_cht==alpha_lo && cht==alpha_hi) ||
                (last_cht==digit && cht!=digit) ||
                (last_cht!=digit && cht==digit)
                    ) && !s.empty())
                        parts[idx++].swap(s);

            if(cht!=padding)
                { s+=toupper(ch); last_cht=cht; }
        }

        if(!s.empty())
            parts[idx++].swap(s);
    }

    std::string& title=meta.title;
    std::string& episode=meta.episode;
    std::string& year=meta.year;

    for(int i=0;i<idx;i++)
    {
        const std::string& s=parts[i];

        if(i>0)
        {
            if(__is_number(s,-1))
            {
                if(s.length()==4)
                    { year=s; break; }
                else if(s.length()>1)
                    break;
            }else if(s=="S" && __is_number(parts[i+1],2))
            {
                episode=s; episode+=parts[i+1];

                const std::string& ss=parts[i+2];
                const std::string& sss=parts[i+3];

                if(ss=="E" && __is_number(sss,2))
                    { episode+=ss; episode+=sss; }
                else
                    episode+="EXX";

                break;
            }
        }

        if(!title.empty())
            title+=' ';

        title+=s;
    }

    return title.empty()?false:true;
}

bool utils::omdb_fetch_meta_data(meta_data& meta)
{
/*
    http://www.omdbapi.com/?t=THE%20LAST+MAN+ON+EARTH&type=movie&y=2014&r=json&v=1
    {
        "Title":"The Last Man on Earth",
        "Year":"2014",
        "Rated":"N/A",
        "Released":"07 Mar 2014",
        "Runtime":"90 min",
        "Genre":"Romance",
        "Director":"Todd Portugal",
        "Writer":"Todd Portugal",
        "Actors":"Madisen Hill, Trip Langley, Rob Mor",
        "Plot":"Uniquely blending romantic comedy & science fiction, \"The Last Man On Earth\" is about two teenagers exploring
            the nature and rejection of first love, when a cataclysmic event leaves them ...",
        "Language":"English",
        "Country":"USA",
        "Awards":"N/A",
        "Poster":"http://ia.media-imdb.com/images/M/MV5BNzE1OTA1MTY3NV5BMl5BanBnXkFtZTgwMDA4NzgzMTE@._V1_SX300.jpg",
        "Metascore":"N/A",
        "imdbRating":"5.8",
        "imdbVotes":"12",
        "imdbID":"tt3585116",
        "Type":"movie",
        "Response":"True"
    }

    http://www.omdbapi.com/?t=THE%20LAST+MAN+ON+EARTH&type=series&r=json&v=1
    {
        "Title":"The Last Man on Earth",
        "Year":"2015–",
        "Rated":"N/A",
        "Released":"01 Mar 2015",
        "Runtime":"30 min",
        "Genre":"Action, Comedy, Romance",
        "Director":"N/A",
        "Writer":"Will Forte",
        "Actors":"Will Forte, Kristen Schaal, January Jones, Mel Rodriguez",
        "Plot":"Almost 2 years after a virus wiped out most of the human race, Phil Miller only wishes for some company, but soon gets more than
            he bargained for when that company shows up in the form of other survivors.",
        "Language":"English",
        "Country":"USA",
        "Awards":"Nominated for 3 Primetime Emmys. Another 4 nominations.",
        "Poster":"http://ia.media-imdb.com/images/M/MV5BMTQ3NTEzODcyNl5BMl5BanBnXkFtZTgwNjY1NzU2NDE@._V1_SX300.jpg",
        "Metascore":"N/A",
        "imdbRating":"7.5",
        "imdbVotes":"27,259",
        "imdbID":"tt3230454",
        "Type":"series",
        "Response":"True"
    }
*/

    return false;
}
