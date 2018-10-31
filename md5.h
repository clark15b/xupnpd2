/* MD5.H - header file for MD5C.C
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
   rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD5 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD5 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */

#ifndef __MD5_H
#define __MD5_H

#include "compat.h"

namespace md5
{
    struct MD5_CTX
    {
        u_int32_t state[4];

        u_int32_t count[2];

        unsigned char buffer[64];
    };

    void MD5_Init(MD5_CTX*);

    void MD5_Update(MD5_CTX*,const unsigned char*,unsigned int);

    void MD5_Final(unsigned char*,MD5_CTX*);
}

#endif
