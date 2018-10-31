/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __SCRIPTING_H
#define __SCRIPTING_H

#include "http.h"

namespace scripting
{
    bool init(void);

    bool main(http::req& req,const std::string& filename);

    void done(void);
}

#endif
