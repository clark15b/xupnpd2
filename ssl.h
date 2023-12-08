/*
 * (c) 2023 Jack'lul <jacklulcat@gmail.com>
 */

#ifndef NO_SSL
#ifndef __SSL_H
#define __SSL_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/rand.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <winsock2.h>
#endif

namespace ssl
{
    bool init(void);

    void done(void);

    SSL* create(SSL_CTX*& ssl_ctx, int fd, const std::string& host);

    SSL_CTX* create_context();

    bool set_verify(SSL_CTX*& ssl_ctx, const std::string& host);

    ssize_t read_with_retry(SSL*& ssl, char* buffer, size_t buffer_size);

    ssize_t write_with_retry(SSL*& ssl, const char* data, size_t data_length);

    void shutdown(SSL*& ssl);

    void shutdown_context(SSL_CTX*& ssl_ctx);

    void handle_error();
}

#endif
#endif
