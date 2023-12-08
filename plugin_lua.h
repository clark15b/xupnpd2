/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __LUAS_H
#define __LUAS_H

#include <string>

namespace luas
{
    void sendurl(const std::string& url);

    std::string translate_url(const std::string& url_translator,const std::string& url,const std::string& method);
}

#endif
