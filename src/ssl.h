
#ifndef SSL_H
#define SSL_H

#include <openssl/ssl.h>

void init_openssl();
void cleanup_openssl();
SSL *create_ssl(int client_fd);

#endif