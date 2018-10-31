/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __DB_H
#define __DB_H

#ifndef NO_SQLITE

#include "db_sqlite.h"

namespace db=db_sqlite;

#else /* NO_SQLITE */

#include "db_mem.h"

namespace db=db_mem;

#endif /* NO_SQLITE */

#endif /* __DB_H */
