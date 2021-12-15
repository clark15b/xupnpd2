/*
 * Copyright (C) 2011-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "soap.h"
#include "soap_int.h"
#include "common.h"
#include "db.h"
#include "mime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>

namespace soap
{
    class retval
    {
    public:
        retval(const std::string& _name,const std::string& _value):name(_name),value(_value) {}

        std::string name; std::string value;
    };

    const char upnp_feature_list[]=
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<Features xmlns=\"urn:schemas-upnp-org:av:avs\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"urn:schemas-upnp-org:av:avs http://www.upnp.org/schemas/av/avs.xsd\">"
            "<Feature name=\"samsung.com_BASICVIEW\" version=\"1\">"
                "<container id=\"1\" type=\"object.item.audioItem\"/>"
                "<container id=\"2\" type=\"object.item.videoItem\"/>"
                "<container id=\"3\" type=\"object.item.imageItem\"/>"
            "</Feature>"
        "</Features>";

    void cds_GetSystemUpdateID(std::map<std::string,std::string>& in,std::list<retval>& out);
    void cds_GetSortCapabilities(std::map<std::string,std::string>& in,std::list<retval>& out);
    void cds_GetSearchCapabilities(std::map<std::string,std::string>& in,std::list<retval>& out);
    void cds_X_GetFeatureList(std::map<std::string,std::string>& in,std::list<retval>& out);
    void cds_X_SetBookmark(std::map<std::string,std::string>& in,std::list<retval>& out);
    void cds_Browse(std::map<std::string,std::string>& in,std::list<retval>& out);
    void cds_Search(std::map<std::string,std::string>& in,std::list<retval>& out);
    void cms_GetCurrentConnectionInfo(std::map<std::string,std::string>& in,std::list<retval>& out);
    void cms_GetProtocolInfo(std::map<std::string,std::string>& in,std::list<retval>& out);
    void cms_GetCurrentConnectionIDs(std::map<std::string,std::string>& in,std::list<retval>& out);
    void msr_IsAuthorized(std::map<std::string,std::string>& in,std::list<retval>& out);
    void msr_RegisterDevice(std::map<std::string,std::string>& in,std::list<retval>& out);
    void msr_IsValidated(std::map<std::string,std::string>& in,std::list<retval>& out);

    typedef void (*soap_method_proc_t)(std::map<std::string,std::string>& in,std::list<retval>& out);

    struct soap_method_t { const char* name; const soap_method_proc_t proc; };

    bool serialize_media(const db::object_t& row,std::string& ss,const std::string& parent_name=std::string());

    void fix_object_id(std::string& objid);

    int search_object_type(const char* exp);

    static const char browse_prefix[]="<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0\">";
    static const char browse_postfix[]="</DIDL-Lite>";

    static const char* schms[]=
    {
        "cds"   , "urn:schemas-upnp-org:service:ContentDirectory:1",
        "cms"   , "urn:schemas-upnp-org:service:ConnectionManager:1",
        "msr"   , "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1",
        NULL    , NULL
    };

    soap_method_t methds[]=
    {
        { "cds::GetSystemUpdateID"        ,     cds_GetSystemUpdateID           },
        { "cds::GetSortCapabilities"      ,     cds_GetSortCapabilities         },
        { "cds::GetSearchCapabilities"    ,     cds_GetSearchCapabilities       },
        { "cds::X_GetFeatureList"         ,     cds_X_GetFeatureList            },
        { "cds::X_SetBookmark"            ,     cds_X_SetBookmark               },
        { "cds::Browse"                   ,     cds_Browse                      },
        { "cds::Search"                   ,     cds_Search                      },

        { "cms::GetCurrentConnectionInfo" ,     cms_GetCurrentConnectionInfo    },
        { "cms::GetProtocolInfo"          ,     cms_GetProtocolInfo             },
        { "cms::GetCurrentConnectionIDs"  ,     cms_GetCurrentConnectionIDs     },

        { "msr::IsAuthorized"             ,     msr_IsAuthorized                },
        { "msr::RegisterDevice"           ,     msr_RegisterDevice              },
        { "msr::IsValidated"              ,     msr_IsValidated                 },

        { NULL                            ,     NULL                            }
    };

    inline void add_esc_char(int ch,std::string& s)
    {
        switch(ch)
        {
        case '<':  s.append("&lt;",4);      break;
        case '>':  s.append("&gt;",4);      break;
        case '&':  s.append("&amp;",5);     break;
        case '\"': s.append("&quot;",6);    break;
        case '\'': s.append("&apos;",6);    break;
        default:   s+=ch;                   break;
        }
    }

    std::string esc(const std::string& s)
    {
        std::string dst;

        for(const unsigned char* p=(unsigned char*)s.c_str();*p;p++)
            add_esc_char(*p,dst);

        return dst;
    }

    std::map<std::string,const char*> schemes;
    std::map<std::string,soap_method_proc_t> methods;
}

bool soap::init(void)
{
    for(int i=0;schms[i];i+=2)
        schemes[schms[i]]=schms[i+1];

    for(int i=0;methds[i].name;i++)
        methods[methds[i].name]=methds[i].proc;

    return true;
}

void soap::done(void) { }

bool soap::main_debug(const std::string& interface,std::map<std::string,std::string>& args,std::string& data_out,const std::string& client_ip)
{
    std::map<std::string,soap_method_proc_t>::const_iterator it=methods.find(interface+"::"+args["method"]);

    if(it==methods.end())
        return false;

    std::list<retval> params_out;

    it->second(args,params_out);

    for(std::list<retval>::const_iterator it=params_out.begin();it!=params_out.end();++it)
    {
        data_out.append(it->name);
        data_out+='=';
        data_out.append(it->value);
        data_out+="\r\n;;\r\n";
    }

    return true;
}

bool soap::main(const std::string& interface,const std::string& method,const std::string& data,std::string& data_out,const std::string& client_ip)
{
    if(interface.empty() || method.empty() || data.empty())
        return false;

    std::map<std::string,std::string> params;

    std::list<retval> params_out;

    {
        node req;

        if(parse(data,req))
            return false;

        node* parms=req.find((std::string("Envelope/Body/")+method).c_str());

        if(!parms)
            return false;

        for(node* p=parms->beg;p;p=p->next)
        {
            if(p->name && p->data && p->len>0)
                params[p->name].assign(p->data,p->len);
        }
    }

    if(utils::is_trace_needed(utils::log_soap))
    {
        std::string s;

        for(std::map<std::string,std::string>::const_iterator it=params.begin();it!=params.end();++it)
            s+=utils::format("%s=>'%s', ",it->first.c_str(),it->second.c_str());
        if(!s.empty())
            s.resize(s.length()-2);

        utils::trace(utils::log_soap,"%s \"%s::%s(%s)\"",client_ip.c_str(),interface.c_str(),method.c_str(),s.c_str());
    }

    static const char err[]=
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
            "<s:Body>"
                "<s:Fault>"
                    "<faultcode>s:Client</faultcode>"
                    "<faultstring>UPnPError</faultstring>"
                        "<detail>"
                            "<u:UPnPError xmlns:u=\"urn:schemas-upnp-org:control-1-0\">"
                                "<u:errorCode>501</u:errorCode>"
                                "<u:errorDescription>Action Failed</u:errorDescription>"
                            "</u:UPnPError>"
                        "</detail>"
                "</s:Fault>"
            "</s:Body>"
        "</s:Envelope>";

    static const char beg[]=
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                "<s:Body>";

    static const char end[]=
                "</s:Body>"
            "</s:Envelope>";

    std::map<std::string,soap_method_proc_t>::const_iterator it=methods.find(interface+"::"+method);

    if(it==methods.end())
        { utils::trace(utils::log_err,"SOAP: method is not found"); data_out.assign(err,sizeof(err)-1); return true; }

    it->second(params,params_out);

    data_out.assign(beg,sizeof(beg)-1);

    char buf[256];

    int n=sprintf(buf,"<u:%sResponse xmlns:u=\"%s\">",method.c_str(),schemes[interface]); data_out.append(buf,n);

    for(std::list<retval>::const_iterator it=params_out.begin();it!=params_out.end();++it)
    {
        data_out+='<'; data_out+=it->name; data_out+='>';

        for(const unsigned char* p=(unsigned char*)it->value.c_str();*p;p++)
            add_esc_char(*p,data_out);

        data_out+="</"; data_out+=it->name; data_out+='>';
    }

    n=sprintf(buf,"</u:%sResponse>",method.c_str()); data_out.append(buf,n);

    data_out.append(end,sizeof(end)-1);

    return true;
}

void soap::cds_GetSystemUpdateID(std::map<std::string,std::string>& in,std::list<retval>& out)
    { out.push_back(retval("Id",utils::format("%d",cfg::system_update_id))); }

void soap::cds_GetSortCapabilities(std::map<std::string,std::string>& in,std::list<retval>& out)
    { out.push_back(retval("SortCaps","dc:title")); }

void soap::cds_GetSearchCapabilities(std::map<std::string,std::string>& in,std::list<retval>& out)
    { out.push_back(retval("SearchCaps","upnp:class")); }

void soap::cds_X_GetFeatureList(std::map<std::string,std::string>& in,std::list<retval>& out)
    { out.push_back(retval("FeatureList",upnp_feature_list)); }

void soap::cds_X_SetBookmark(std::map<std::string,std::string>& in,std::list<retval>& out)
    { /* in["ObjectID"], in["PosSecond"]; */ }


void soap::cms_GetCurrentConnectionInfo(std::map<std::string,std::string>& in,std::list<retval>& out)
{
    out.push_back(retval("ConnectionID","0"));
    out.push_back(retval("RcsID","-1"));
    out.push_back(retval("AVTransportID","-1"));
    out.push_back(retval("ProtocolInfo",""));
    out.push_back(retval("PeerConnectionManager",""));
    out.push_back(retval("PeerConnectionID","-1"));
    out.push_back(retval("Direction","Output"));
    out.push_back(retval("Status","OK"));
}

// wget --no-proxy -q -O - http://127.0.0.1:4044/ctrl/cms?method=GetProtocolInfo
void soap::cms_GetProtocolInfo(std::map<std::string,std::string>& in,std::list<retval>& out)
{
    out.push_back(retval("Sink",""));
    out.push_back(retval("Source",""));

    std::string& s=out.back().value;

    for(int i=0;mime::types[i].name;i++)
    {
        if(mime::types[i].upnp_proto)
            { s+=mime::types[i].upnp_proto; s+="*,"; }
    }

    if(!s.empty() && s[s.length()-1]==',')
        s.resize(s.length()-1);
}

void soap::cms_GetCurrentConnectionIDs(std::map<std::string,std::string>& in,std::list<retval>& out)
    { out.push_back(retval("ConnectionIDs","")); }

void soap::msr_IsAuthorized(std::map<std::string,std::string>& in,std::list<retval>& out)
    { out.push_back(retval("Result","1")); }

void soap::msr_RegisterDevice(std::map<std::string,std::string>& in,std::list<retval>& out) {}

void soap::msr_IsValidated(std::map<std::string,std::string>& in,std::list<retval>& out)
    { out.push_back(retval("Result","1")); }

bool soap::serialize_media(const db::object_t& row,std::string& ss,const std::string& parent_name)
{
    int objtype=row.objtype;

    if(objtype==mime::cont)
    {
        char buf[1024];

        int n=snprintf(buf,sizeof(buf),"<container id=\"%s\" childCount=\"%s\" parentID=\"%s\" restricted=\"true\"><dc:title>%s</dc:title><upnp:class>%s</upnp:class></container>",
            row.objid.c_str(),row.items.c_str(),row.parentid.c_str(),esc(row.name).c_str(),mime::upnp_container);

        if(n==-1 || n>=sizeof(buf))
            n=sizeof(buf)-1;

        ss.append(buf,n);
    }else
    {
        int mimecode=row.mimecode;

        if(mimecode<0)
            return false;

        mime::type_t* t=mime::get_by_id(mimecode);

        if(!t || !t->upnp_proto)
            return false;

        std::string artist,logo;

        if(!parent_name.empty())
        {
            if(objtype==mime::video)
                utils::format(artist,"<upnp:actor>%s</upnp:actor>",parent_name.c_str());
            else if(objtype==mime::audio)
                utils::format(artist,"<upnp:artist>%s</upnp:artist>",parent_name.c_str());
        }

        const std::string& _logo=row.logo;

        if(!_logo.empty())
            utils::format(logo,"<upnp:albumArtURI dlna:profileID=\"%s\">%s/%s</upnp:albumArtURI>",cfg::upnp_logo_profile.c_str(),cfg::www_location.c_str(),_logo.c_str());

        std::string length(row.length);

        const std::string& handler=row.handler;

        if(length.empty())
            length=cfg::upnp_live_length;

        char buf[1024];

        int n=snprintf(buf,sizeof(buf),
            "<item id=\"%s\" parentID=\"%s\" restricted=\"true\"><dc:title>%s</dc:title><upnp:class>%s</upnp:class>%s%s<res size=\"%s\""
            " protocolInfo=\"%s%s\">%s/stream/%s.%s</res></item>",
            row.objid.c_str(),row.parentid.c_str(),esc(row.name).c_str(),t->upnp_type,
            artist.c_str(),logo.c_str(),length.c_str(),t->upnp_proto,t->dlna_extras,cfg::www_location.c_str(),
                row.uuid.empty()?row.objid.c_str():row.uuid.c_str(),t->name);

        ss.append(buf,n);
    }

    return true;
}

void soap::fix_object_id(std::string& objid)
{
    if(objid.length()<32 && atoi(objid.c_str())<=cfg::upnp_objid_offset)
        objid="0";
}

// wget --no-proxy -q -O - 'http://127.0.0.1:4044/ctrl/cds?method=Browse&ObjectID=0&BrowseFlag=BrowseMetadata&StartingIndex=0&RequestedCount=0'
// wget --no-proxy -q -O - 'http://127.0.0.1:4044/ctrl/cds?method=Browse&ObjectID=0&StartingIndex=0&RequestedCount=0'
void soap::cds_Browse(std::map<std::string,std::string>& in,std::list<retval>& out)
{
    std::string ss;

    std::string objid=in["ObjectID"];                   // '0' - root

    if(objid.empty())
        objid=in["ContainerID"];

    fix_object_id(objid);

    const std::string& flag=in["BrowseFlag"];           // BrowseDirectChildren, BrowseMetadata
    const std::string& filter=in["Filter"];             // 'id,dc:title,res,sec:CaptionInfo,sec:CaptionInfoEx,pv:subtitlefile'

    int offset=atoi(in["StartingIndex"].c_str());       // 0 - first
    int count=atoi(in["RequestedCount"].c_str());       // 0 - all

    int total=0;
    int count_matches=0;

    ss.append(browse_prefix,sizeof(browse_prefix)-1);

    db::object_t row;

    db::locker lock;

    if(db::find_object_by_id(objid,row) && !row.empty())
    {
        if(flag=="BrowseMetadata")
        {
            if(serialize_media(row,ss))
                total=count_matches=1;
        }else
        {
            total=atoi(row.items.c_str());

            std::string parent_name=row.name;

            if(count<0)
                count=0;

            if(offset<0)
                offset=0;

            if(count==0)
            {
                count=total-offset;

                if(count<0)
                    count=0;
            }

            db::rowset_t stmt;

            if(db::find_objects_by_parent_id(objid,count,offset,stmt))
            {
                const db::object_t* row;

                while((row=stmt.fetch()))
                {
                    if(serialize_media(*row,ss/*,parent_name*/))
                        count_matches++;
                }
            }
        }
    }

    ss.append(browse_postfix,sizeof(browse_postfix)-1);

    out.push_back(retval("Result","")); out.back().value.swap(ss);
    out.push_back(retval("NumberReturned",utils::format("%d",count_matches)));
    out.push_back(retval("TotalMatches",utils::format("%d",total)));
    out.push_back(retval("UpdateID",utils::format("%d",cfg::system_update_id)));
}

// 0 - all, 1 - video, 2 - audio, 3 - images, -1 - error
int soap::search_object_type(const char* exp)
{
    static const char class_tag[]     = "upnp:class";
    static const char derived_tag[]   = "derivedfrom";

    static const char video_tag[]     = "object.item.videoItem";
    static const char audio_tag[]     = "object.item.audioItem";
    static const char image_tag[]     = "object.item.imageItem";

    while(*exp && *exp==' ')
        exp++;

    if(!*exp || !strcmp(exp,"*"))
        return 0;

    const char* p=strstr(exp,class_tag);

    if(p)
    {
        p+=sizeof(class_tag)-1;

        while(*p && (*p==' ' || *p=='\t'))
            p++;

        int ok=1;

        if(!strncmp(p,derived_tag,sizeof(derived_tag)-1))
            p+=sizeof(derived_tag)-1;
        else if(*p=='=')
            p++;
        else
            ok=0;

        if(ok)
        {
            while(*p && (*p==' ' || *p=='\t'))
                p++;
            if(*p=='\"')
            {
                p++;

                const char* p2=strchr(p,'\"');

                if(p2)
                {
                    char tmp[64];

                    int n=p2-p;

                    if(n>=sizeof(tmp))
                        n=sizeof(tmp)-1;

                    strncpy(tmp,p,n);
                    tmp[n]=0;

                    if(!strncmp(tmp,video_tag,sizeof(video_tag)-1))
                        return mime::video;
                    else if(!strncmp(tmp,audio_tag,sizeof(audio_tag)-1))
                        return mime::audio;
                    else if(!strncmp(tmp,image_tag,sizeof(image_tag)-1))
                        return mime::image;
                }
            }
        }
    }

    return -1;
}

// wget --no-proxy -q -O - 'http://127.0.0.1:4044/ctrl/cds?method=Search&ContainerID=0&StartingIndex=0&RequestedCount=0&SearchCriteria=*'
void soap::cds_Search(std::map<std::string,std::string>& in,std::list<retval>& out)
{
    std::string ss;

    std::string objid=in["ContainerID"];

    fix_object_id(objid);

    int what=search_object_type(in["SearchCriteria"].c_str());

    int offset=atoi(in["StartingIndex"].c_str());
    int count=atoi(in["RequestedCount"].c_str());

    int total=0;
    int count_matches=0;

    ss.append(browse_prefix,sizeof(browse_prefix)-1);

    if(what>=0)
    {
        db::locker lock;

        db::object_t row1,row2;

        if(db::find_object_by_id(objid,row1) && !row1.empty() && row1.objtype==0)
        {
            db::find_next_object_by_id(row1.objid,row1.parentid,row2);

            const std::string& from=row1.objid;
            const std::string& to=row2.objid;

            total=db::get_objects_count(from,to,what);

            if(total>0)
            {
                if(offset<0)
                    offset=0;

                if(count<0)
                    count=0;

                if(count==0)
                {
                    count=total-offset;

                    if(count<0)
                        count=0;
                }

                db::rowset_t stmt;

                if(db::search_objects(from,to,what,count,offset,stmt))
                {
                    const db::object_t* row;

                    while((row=stmt.fetch()))
                    {
                        if(serialize_media(*row,ss))
                            count_matches++;
                    }
                }
            }
        }
    }

    ss.append(browse_postfix,sizeof(browse_postfix)-1);

    out.push_back(retval("Result","")); out.back().value.swap(ss);
    out.push_back(retval("NumberReturned",utils::format("%d",count_matches)));
    out.push_back(retval("TotalMatches",utils::format("%d",total)));
    out.push_back(retval("UpdateID",utils::format("%d",cfg::system_update_id)));
}
