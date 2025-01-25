#ifndef SERVER_H
#define SERVER_H
#include <openssl/ssl.h>

// Main request handler
void handle_client(SSL *ssl);

#endif
