/*
 * (c) 2023 Jack'lul <jacklulcat@gmail.com>
 */

#ifndef NO_SSL

#include "common.h"
#include "ssl.h"

bool ssl::init(void)
{
    if (SSL_library_init() != 1 || SSL_load_error_strings() != 1) {
        { utils::trace(utils::log_err,"OpenSSL initialize fail"); return false; }
    }

    OpenSSL_add_all_algorithms();

    return true;
}

void ssl::done(void)
{
    EVP_cleanup();
    ERR_free_strings();
    CRYPTO_cleanup_all_ex_data();
}

SSL* ssl::create(SSL_CTX*& ssl_ctx, int fd, const std::string& host)
{
    if (ssl_ctx != NULL) {
        SSL* ssl = SSL_new(ssl_ctx);

        if(fd == INVALID_SOCKET) {
            utils::trace(utils::log_err, "SSL object creation error - invalid socket");
            return NULL;
        }

        SSL_set_fd(ssl, fd);

        if (!host.empty())
            SSL_set_tlsext_host_name(ssl, host.c_str());

        if (SSL_connect(ssl) <= 0) {
            ssl::handle_error();

            return NULL;
        }

        return ssl;
    }
    else
        utils::trace(utils::log_err, "SSL object creation error - no context");

    return NULL;
}

SSL_CTX* ssl::create_context()
{
    SSL_CTX* ssl_ctx = SSL_CTX_new(SSLv23_client_method());

    if (!ssl_ctx) {
        ssl::handle_error();

        return NULL;
    }

    return ssl_ctx;
}

bool ssl::set_verify(SSL_CTX*& ssl_ctx, const std::string& host)
{
    if (!cfg::openssl_verify) {
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
    } else {
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
        SSL_CTX_set_verify_depth(ssl_ctx, 1);

        if (host.empty())
            return false;

        const char* host_cstr = host.c_str();
        SSL_CTX_ctrl(ssl_ctx, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name, (void*)host_cstr);

        if (!cfg::openssl_ca_location.empty()) {
            if (SSL_CTX_load_verify_locations(ssl_ctx, cfg::openssl_ca_location.c_str(), NULL) != 1) {
                ssl::handle_error();

                return false;
            }
        } else {
            if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
                ssl::handle_error();

                return false;
            }
        }
    }

    return true;
}

ssize_t ssl::read_with_retry(SSL*& ssl, char* buffer, size_t buffer_size)
{
    ssize_t n;
    int ssl_read_result;

    while ((ssl_read_result = SSL_read(ssl, buffer, buffer_size)) <= 0) {
        int ssl_error = SSL_get_error(ssl, ssl_read_result);

        if (ssl_error == SSL_ERROR_WANT_READ) {
            continue;
        } else {
            ssl::handle_error();
            break;
        }
    }

    return ssl_read_result;
}

ssize_t ssl::write_with_retry(SSL*& ssl, const char* data, size_t data_length)
{
    ssize_t n;
    int ssl_write_result;

    while ((ssl_write_result = SSL_write(ssl, data, data_length)) <= 0) {
        int ssl_error = SSL_get_error(ssl, ssl_write_result);

        if (ssl_error == SSL_ERROR_WANT_WRITE) {
            continue;
        } else {
            ssl::handle_error();
            break;
        }
    }

    return ssl_write_result;
}

void ssl::shutdown(SSL*& ssl)
{
    if (ssl != nullptr)
    {
        if (SSL_shutdown(ssl) < 0)
            ssl::handle_error();

        SSL_free(ssl);
        ssl = nullptr;
    }
}

void ssl::shutdown_context(SSL_CTX*& ssl_ctx)
{
    if (ssl_ctx != nullptr)
    {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = nullptr;
    }
}

void ssl::handle_error()
{
    unsigned long err;

    while ((err = ERR_get_error()) > 0) {
        char error[256];
        ERR_error_string(err, error);
        std::string error_string(error);
        utils::trace(utils::log_err, "SSL error: %s", error_string.c_str());
    }
}

#endif
