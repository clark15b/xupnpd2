/*
 * (c) 2023 Jack'lul <jacklulcat@gmail.com>
 */

#ifndef NO_SSL
#ifndef __HLS_DECRYPT_H
#define __HLS_DECRYPT_H

#include "plugin_hls_common.h"

namespace hls_decrypt
{
    using namespace hls;

    void sendurl(const std::string& url);

    std::string download_key(const std::string& key_url);

    bool decrypt(const hls::chunk_info& c, const char* input, char* output, int* length);

    bool decrypt_AES128(const unsigned char* key, const unsigned char* iv, const char* input, char* output, int* length);
}

#endif
#endif
