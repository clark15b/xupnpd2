/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __SSDP_H
#define __SSDP_H

#include "common.h"

#ifndef _WIN32
#include <netinet/in.h>
#endif /* _WIN32 */

namespace ssdp
{
    bool init(void);

    enum announce_t { alive = 1, byebye = 2 };

    bool send_announce(announce_t type);

    void process_query(void);

    in_addr_t get_lan_if_addr(void);

    socket_t get_incoming_socket(void);

    void close_all_sockets(void);

    void done(void);
}

#endif
