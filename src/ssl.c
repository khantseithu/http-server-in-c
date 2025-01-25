
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdlib.h>
#include <stdio.h>
#include "ssl.h"

#define CERT_FILE "certs/cert.pem"
#define KEY_FILE  "certs/key.pem"

static SSL_CTX *ssl_ctx = NULL;

void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
        perror("Unable to create SSL context");
        exit(EXIT_FAILURE);
    }

    // Load certificate/key
    if (SSL_CTX_use_certificate_file(ssl_ctx, CERT_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, KEY_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void cleanup_openssl() {
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }
    EVP_cleanup();
}

SSL *create_ssl(int client_fd) {
    SSL *ssl = SSL_new(ssl_ctx);
    SSL_set_fd(ssl, client_fd);
    return ssl;
}