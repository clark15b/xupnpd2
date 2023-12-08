/*
 * (c) 2023 Jack'lul <jacklulcat@gmail.com>
 */

#ifndef NO_SSL

#include "plugin_hls_decrypt.h"
#include "plugin_hls_common.h"
#include "plugin_hls.h"
#include <string>
#include <algorithm>
#include <sstream>
#include <openssl/aes.h>
#include "common.h"

#ifndef _WIN32
#include <unistd.h>
#else /* _WIN32 */
#include <io.h>
#endif /* _WIN32 */

// based on hls::sendurl
void hls_decrypt::sendurl(const std::string& url)
{
    int memory_limit = 999999999;

    const char* opts = getenv("OPTS");

    if(opts && *opts)
    {
        char* endptr = NULL;

        memory_limit = (int)strtol(opts, &endptr, 10);

        if(*endptr == 'k' || *endptr == 'K')
            memory_limit *= 1024;
        else if(*endptr == 'm' || *endptr == 'M')
            memory_limit *= 1024 * 1024;
    }

    hls::context ctx;
    std::string stream_url, user_agent;
    int stream_id = 0;

    hls::split_url(url, stream_url, user_agent, &stream_id);

    hls::chunks_list chunks;

#ifndef _WIN32
        timeval last_update_tv;
#else
        hls::timeval last_update_tv;
#endif

    hls::tv_now(last_update_tv);

    bool quit = false;

    while (!quit)
    {
        unsigned long target_duration = chunks.get_target_duration();
        unsigned long elapsed = hls::elapsed_usec(last_update_tv);

        if (elapsed >= target_duration)
        {
            hls::tv_now(last_update_tv);

            if (!hls::update_stream_info(stream_url, stream_id, user_agent, chunks,ctx))
                break;
        }

        while (chunks.empty())
        {
            unsigned long delay = chunks.get_target_duration() / 2;

            if (delay < 1000000)
                delay = 1000000;

            usleep(delay);

            hls::tv_now(last_update_tv);

            if (!hls::update_stream_info(stream_url, stream_id, user_agent, chunks,ctx))
                break;
        }

        if (chunks.empty() || !chunks.get_target_duration())
            break;

        hls::chunk_info c;

        chunks.pop_front(c);

        hls::stream s(user_agent);

        if (s.open(c.url))
        {
            char buf[4096];
            long bytes_left = (long)s.length();

            if (!c.key_method.empty() && c.key_method != "NONE") { // chunk is encrypted
                // memory usage will double when decrypting so we can only use half of the limit when reading
                if (bytes_left < 1)
                    bytes_left = memory_limit / 2;

                if (bytes_left > memory_limit / 2) {
                    utils::trace(utils::log_err, "data length exceeded memory limit (%d/%d)", bytes_left, memory_limit / 2);
                    s.close();
                    return;
                }

                char *data_encrypted = nullptr;
                int total_size = 0;

                while (bytes_left > 0)
                {
                    int n = s.read(buf, bytes_left > sizeof(buf) ? sizeof(buf) : bytes_left);

                    if (n < 1)
                        break;

                    data_encrypted = static_cast<char*>(realloc(data_encrypted, total_size + n));

                    if (data_encrypted == nullptr) {
                        utils::trace(utils::log_err, "failed to allocate memory (%d)", total_size + n);
                        return;
                    }

                    memcpy(data_encrypted + total_size, buf, n);

                    total_size += n;
                    bytes_left -= n;
                }

                s.close();

                if (bytes_left > 0) {
                    utils::trace(utils::log_err, "failed to read all data (%d bytes left)", bytes_left);
                    return;
                }

                char *data_decrypted = nullptr;
                data_decrypted = static_cast<char*>(realloc(data_decrypted, total_size));

                if (data_decrypted == nullptr) {
                    utils::trace(utils::log_err, "failed to allocate memory (%d)", total_size);
                    return;
                }

                if (!hls_decrypt::decrypt(c, data_encrypted, data_decrypted, &total_size)) {
                    utils::trace(utils::log_err, "failed to decrypt the data");
                    return;
                }

                free(data_encrypted); // free the allocated memory

                bytes_left = total_size;
                int bytes_sent = 0;

                while (bytes_left > 0)
                {
                    int n = ::write(1, data_decrypted + bytes_sent, bytes_left > sizeof(buf) ? sizeof(buf) : bytes_left);

                    if(n < 1)
                        break;

                    bytes_sent += n;
                    bytes_left -= n;
                }

                free(data_decrypted); // free the allocated memory

                if (bytes_left > 0) {
                    utils::trace(utils::log_err, "failed to write all data (%d bytes left)", bytes_left);
                    return;
                }
            } else { // chunk is not encrypted
                if (bytes_left < 1)
                    bytes_left = 999999999;

                while (bytes_left > 0)
                {
                    int n = s.read(buf, bytes_left > sizeof(buf) ? sizeof(buf) : bytes_left);

                    if(n < 1)
                        break;

                    bytes_left -= n;

                    int bytes_sent = 0;

                    while (bytes_sent < n)
                    {
                        int m = ::write(1, buf + bytes_sent, n - bytes_sent);

                        if(m < 1) {
                            quit = true;
                            break;
                        }

                        bytes_sent += m;
                    }

                    if (quit)
                        break;
                }

                s.close();
            }
        }
    }
}

std::string hls_decrypt::download_key(const std::string& key_url)
{
    //utils::trace(utils::log_debug, "fetching decryption key: %s", key_url.c_str());

    std::string key_data;

    int result = http::fetch(key_url, key_data, "");

    if (result != 0) {
        return "";
    }

    return key_data;
}

bool hls_decrypt::decrypt(const hls::chunk_info& c, const char* input, char* output, int* length)
{
    if (!c.key_method.empty() && !c.key_url.empty()) {
        if (c.key_method == "NONE") { // we shouldn't be here!
            utils::trace(utils::log_err, "decryption function called with decryption method set to NONE");
            strcpy(output, input);
            return true;
        }

        if (c.key_method != "AES-128") {
            utils::trace(utils::log_err, "unsupported encryption method: %s", c.key_method.c_str());
            return false;
        }

        std::string key_data;
        std::string key_iv = c.key_iv;

        if ((key_data = hls_decrypt::download_key(c.key_url)) == "") {
            utils::trace(utils::log_err, "failed to fetch decryption key: %s", c.key_url.c_str());
            return false;
        }

        if (key_iv.empty()) {
            key_iv = c.id; // if the IV is not provided then Media Sequence Number is the IV
        }

        return hls_decrypt::decrypt_AES128(reinterpret_cast<const unsigned char*>(key_data.c_str()),
                                           reinterpret_cast<const unsigned char*>(key_iv.c_str()),
                                           input, output, length);
    }

    return false;
}

bool hls_decrypt::decrypt_AES128(const unsigned char* key, const unsigned char* iv, const char* input, char* output, int* length)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_init(ctx);

    if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv)) {
        EVP_CIPHER_CTX_free(ctx);
        ssl::handle_error();
        return false;
    }

    //utils::trace(utils::log_debug, "decryption init   -  input length = %d", *length);

    int out_len;

    if (!EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(output), &out_len, reinterpret_cast<const unsigned char*>(input), *length)) {
        EVP_CIPHER_CTX_free(ctx);
        ssl::handle_error();
        return false;
    }

    //utils::trace(utils::log_debug, "decryption update - output length = %d", *length, out_len);

    int final_len;

    if (!EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(output + out_len), &final_len)) {
        EVP_CIPHER_CTX_free(ctx);
        ssl::handle_error();
        return false;
    }

    out_len += final_len;
    *length = out_len;

    //utils::trace(utils::log_debug, "decryption final  - output length = %d  (final length = %d)", out_len, final_len);

    EVP_CIPHER_CTX_free(ctx);

    return true;
}

#endif
