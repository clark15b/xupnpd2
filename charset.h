/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __CHARSET_H
#define __CHARSET_H

#include <string>

namespace charset
{
    bool set(const std::string& cp);

    std::string to_utf8(const std::string& s);
}

#endif
