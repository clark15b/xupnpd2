/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "mime.h"
#include <string>
#include <map>

namespace mime
{
    const char upnp_container[] = "object.container";
//    const char upnp_container[] = "object.container.storageFolder";
    const char upnp_video[]     = "object.item.videoItem";
//    const char upnp_video[]     = "object.item.videoItem.movie";
    const char upnp_audio[]     = "object.item.audioItem.musicTrack";
    const char upnp_image[]     = "object.item.imageItem";
    const char dlna_ext_none[]  = "*";

    type_t types[]=
    {
        // text
        { 0,   undef, "html",  "htm", "text/html",                     NULL,                                        NULL,       dlna_ext_none },
        { 1,   undef, "txt",   NULL,  "text/plain",                    NULL,                                        NULL,       dlna_ext_none },
        { 2,   undef, "xml",   NULL,  "text/xml; charset=\"UTF-8\"",   NULL,                                        NULL,       dlna_ext_none },
        { 3,   undef, "srt",   NULL,  "video/subtitle",                NULL,                                        NULL,       dlna_ext_none },
        { 4,   undef, "css",   NULL,  "text/css",                      NULL,                                        NULL,       dlna_ext_none },
        { 5,   undef, "json",  NULL,  "application/json",              NULL,                                        NULL,       dlna_ext_none },
        { 6,   undef, "js",    NULL,  "application/javascript",        NULL,                                        NULL,       dlna_ext_none },
        { 7,   undef, "ttf",   "ttfc","application/x-font-ttf",        NULL,                                        NULL,       dlna_ext_none },
        { 8,   undef, "eot",   NULL,  "application/vnd.ms-fontobject", NULL,                                        NULL,       dlna_ext_none },
        { 9,   undef, "woff",  NULL,  "application/font-woff",         NULL,                                        NULL,       dlna_ext_none },
        { 10,  undef, "pdf",   NULL,  "application/pdf",               NULL,                                        NULL,       dlna_ext_none },
        { 11,  undef, "m3u8",  NULL,  "application/x-mpegURL",         NULL,                                        NULL,       dlna_ext_none },
        { 12,  undef, "m3u",   NULL,  "audio/x-mpegurl",               NULL,                                        NULL,       dlna_ext_none },

        // image
        { 13,  image, "ico",   NULL,  "image/vnd.microsoft.icon",      NULL,                                        upnp_image, dlna_ext_none },
        { 14,  image, "jpeg",  "jpg", "image/jpeg",                    "http-get:*:image/jpeg:",                    upnp_image, dlna_ext_none },
        { 15,  image, "png",   NULL,  "image/png",                     "http-get:*:image/png:",                     upnp_image, dlna_ext_none },
        { 16,  image, "gif",   NULL,  "image/gif",                     "http-get:*:image/gif:",                     upnp_image, dlna_ext_none },
        { 17,  image, "tiff",  "tif", "image/tiff",                    "http-get:*:image/tiff:",                    upnp_image, dlna_ext_none },
        { 18,  image, "bmp",   NULL,  "image/bmp",                     "http-get:*:image/bmp:",                     upnp_image, dlna_ext_none },
        { 19,  image, "pcx",   NULL,  "image/pcx",                     NULL,                                        upnp_image, dlna_ext_none },
        { 20,  image, "sgi",   NULL,  "image/sgi",                     NULL,                                        upnp_image, dlna_ext_none },
        { 21,  image, "svg",   "svgz","image/svg+xml",                 NULL,                                        upnp_image, dlna_ext_none },

        // archive
        { 22,  undef, "bz",    NULL,  "application/x-bzip",            NULL,                                        NULL,       dlna_ext_none },
        { 23,  undef, "bz2",   NULL,  "application/x-bzip2",           NULL,                                        NULL,       dlna_ext_none },
        { 24,  undef, "cab",   NULL,  "application/x-cab",             NULL,                                        NULL,       dlna_ext_none },
        { 25,  undef, "tgz",   NULL,  "application/x-gtar-compressed", NULL,                                        NULL,       dlna_ext_none },
        { 26,  undef, "gz",    NULL,  "application/x-gzip",            NULL,                                        NULL,       dlna_ext_none },
        { 27,  undef, "zip",   NULL,  "application/zip",               NULL,                                        NULL,       dlna_ext_none },
        { 28,  undef, "rar",   NULL,  "application/rar",               NULL,                                        NULL,       dlna_ext_none },

        // video
        { 29,  video, "avi",   "divx","video/x-msvideo",               "http-get:*:video/avi:",                     upnp_video, dlna_ext_none },
        { 30,  video, "asf",   "asx", "video/x-ms-asf",                "http-get:*:video/x-ms-asf:",                upnp_video, dlna_ext_none },
        { 31,  video, "wmv",   NULL,  "video/x-ms-wmv",                "http-get:*:video/x-ms-wmv:",                upnp_video, dlna_ext_none },
        { 32,  video, "mp4",   "m4v", "video/mp4",                     "http-get:*:video/mp4:",                     upnp_video, dlna_ext_none },
        { 33,  video, "mpeg",  "mpg", "video/mpeg",                    "http-get:*:video/mpeg:",                    upnp_video, dlna_ext_none },
        { 34,  video, "m2ts",  "mts", "video/MP2T",                    "http-get:*:video/vnd.dlna.mpeg-tts:",       upnp_video, dlna_ext_none },
        { 35,  video, "ts",    NULL,  "video/MP2T",                    "http-get:*:video/vnd.dlna.mpeg-tts:",       upnp_video, dlna_ext_none },
        { 36,  video, "mov",   "qt",  "video/quicktime",               "http-get:*:video/quicktime:",               upnp_video, dlna_ext_none },
        { 37,  video, "mkv",   NULL,  "video/x-matroska",              "http-get:*:video/x-matroska:",              upnp_video, dlna_ext_none },
        { 38,  video, "3gp",   NULL,  "video/3gpp",                    "http-get:*:video/3gpp:",                    upnp_video, dlna_ext_none },
        { 39,  video, "flv",   NULL,  "video/x-flv",                   "http-get:*:video/x-flv:",                   upnp_video, dlna_ext_none },
        { 40,  video, "webm",  NULL,  "video/webm",                    "http-get:*:video/webm:",                    upnp_video, dlna_ext_none },
        { 41,  video, "vob",   NULL,  "video/x-ms-vob",                "http-get:*:video/x-ms-vob:",                upnp_video, dlna_ext_none },
        { 42,  video, "ogv",   NULL,  "video/ogg",                     "http-get:*:video/x-ogg:",                   upnp_video, dlna_ext_none },

        // audio
        { 43,  audio, "aac",   NULL,  "audio/x-aac",                   "http-get:*:audio/x-aac:",                   upnp_audio, dlna_ext_none },
        { 44,  audio, "ac3",   NULL,  "audio/ac3",                     "http-get:*:audio/x-ac3:",                   upnp_audio, dlna_ext_none },
        { 45,  audio, "m2a",   NULL,  "audio/mpeg",                    "http-get:*:audio/mpeg:",                    upnp_audio, dlna_ext_none },
        { 46,  audio, "mp3",   NULL,  "audio/mpeg",                    "http-get:*:audio/mp3:",                     upnp_audio, dlna_ext_none },
        { 47,  audio, "ogg",   "oga", "audio/ogg",                     "http-get:*:audio/x-ogg:",                   upnp_audio, dlna_ext_none },
        { 48,  audio, "wma",   NULL,  "audio/x-ms-wma",                "http-get:*:audio/x-ms-wma:",                upnp_audio, dlna_ext_none },
        { 49,  audio, "mka",   NULL,  "audio/x-matroska",              "http-get:*:audio/x-matroska:",              upnp_audio, dlna_ext_none },
        { 50,  audio, "wav",   NULL,  "audio/x-wav",                   "http-get:*:audio/wav:",                     upnp_audio, dlna_ext_none },
        { 51,  audio, "flac",  NULL,  "audio/x-flac",                  "http-get:*:audio/x-flac:",                  upnp_audio, dlna_ext_none },
        { 52,  audio, "weba",  NULL,  "audio/webm",                    "http-get:*:audio/webm:",                    upnp_audio, dlna_ext_none },

        { -1,  undef, NULL,    NULL,  NULL,                            NULL,                                        NULL,       NULL }
    };

    std::map<std::string,type_t*> mimes;
}

bool mime::init(void)
{
    for(int i=0;types[i].name;i++)
    {
        mimes[types[i].name]=types+i;

        if(types[i].alias)
            mimes[types[i].alias]=types+i;
    }

    return true;
}

void mime::disable_dlna_extras(void)
{
    for(int i=0;types[i].name;i++)
        types[i].dlna_extras=dlna_ext_none;
}

mime::type_t* mime::get_by_name(const std::string& name)
{
    std::map<std::string,type_t*>::const_iterator it=mimes.find(name);

    if(it==mimes.end())
        return NULL;

    return it->second;
}

mime::type_t* mime::get_by_id(int id)
{
    if(id<0 || id>=(sizeof(types)/sizeof(type_t))-1)
        return NULL;

    return types+id;
}

void mime::done(void) { }
