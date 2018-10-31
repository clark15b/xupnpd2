/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include <stdlib.h>

void* operator new(unsigned int n) { return malloc(n); }

void operator delete(void* p) { free(p); }

void operator delete[](void* p) { free(p); }
