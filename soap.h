/*
 * Copyright (C) 2011-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __SOAP_H
#define __SOAP_H

#include <string>
#include <map>

namespace soap
{
    bool init(void);

    void done(void);

    bool main(const std::string& interface,const std::string& method,const std::string& data,std::string& data_out,const std::string& client_ip);

    bool main_debug(const std::string& interface,std::map<std::string,std::string>& args,std::string& data_out,const std::string& client_ip);
}

#endif
