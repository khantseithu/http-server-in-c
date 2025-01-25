
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/wait.h> // For waitpid()
#include <string.h>   // For memset()
#include "ssl.h"      // For init_openssl, cleanup_openssl
#include "server.h"   // For handle_client

#define PORT 8080
#define BACKLOG 10

int main(void) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Reuse address/port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 2. Bind to port
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Listen
    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", PORT);

    signal(SIGCHLD, SIG_IGN); // Avoid zombie processes
    init_openssl();           // From ssl.h

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_fd == -1) {
            perror("accept failed");
            continue;
        }

        SSL *ssl = create_ssl(client_fd); // from ssl.h

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            close(client_fd);
            SSL_free(ssl);
            continue;
        } else if (pid == 0) {
            // Child process
            close(server_fd);
            if (SSL_accept(ssl) <= 0) {
                ERR_print_errors_fp(stderr);
            } else {
                handle_client(ssl); // handle request/response
            }
            SSL_free(ssl);
            close(client_fd);
            exit(0);
        } else {
            // Parent
            close(client_fd);
            SSL_free(ssl);
        }
    }

    cleanup_openssl();
    close(server_fd);
    return 0;
}