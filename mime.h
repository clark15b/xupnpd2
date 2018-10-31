/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __MIME_H
#define __MIME_H

#include "mime.h"
#include <string>
#include <map>

namespace mime
{
    struct type_t
    {
        int id;
        int type;
        const char* name;
        const char* alias;
        const char* mime;
        const char* upnp_proto;
        const char* upnp_type;
        const char* dlna_extras;
    };

    enum
    {
        undef   = 0,
        cont    = 0,
        video   = 1,
        audio   = 2,
        image   = 3
    };

    extern const char upnp_container[];
    extern const char upnp_video[];
    extern const char upnp_audio[];
    extern const char upnp_image[];

    extern type_t types[];

    bool init(void);

    void disable_dlna_extras(void);

    type_t* get_by_name(const std::string& name);

    type_t* get_by_id(int id);

    void done(void);
}


#endif
