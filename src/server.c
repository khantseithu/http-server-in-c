#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "server.h"
#include "utils.h" // For is_path_safe, get_mime_type

SSL_CTX *ssl_ctx; // Global SSL context

#define CERT_FILE "certs/cert.pem"
#define KEY_FILE "certs/key.pem"


#define PORT 8080
#define BUFFER_SIZE 4096


void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    ssl_ctx = SSL_CTX_new(TLS_server_method());
    
    // Load certificate and key
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
    SSL_CTX_free(ssl_ctx);
    EVP_cleanup();
}

// Parse the HTTP request line (e.g., "GET / HTTP/1.1")
static int parse_request_line(const char *request_line, char *method, char *path) {
    // ...existing code...
    char line_copy[BUFFER_SIZE];
    strncpy(line_copy, request_line, BUFFER_SIZE);
    line_copy[BUFFER_SIZE-1] = '\0';

    char *token = strtok(line_copy, " ");
    if (!token) return -1;
    strcpy(method, token);

    token = strtok(NULL, " ");
    if (!token) return -1;
    strcpy(path, token);

    return 0;
}

static void serve_error(SSL *ssl, int status_code, const char *status_text) {
    char error_path[BUFFER_SIZE];
    snprintf(error_path, BUFFER_SIZE, "public/%d.html", status_code);

    // Try to open the custom error page
    int file_fd = open(error_path, O_RDONLY);
    if (file_fd == -1) {
        // Fallback to plain text
        char *response_template = 
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %zu\r\n\r\n"
            "%d %s";
        char response[BUFFER_SIZE];
        int len = snprintf(response, BUFFER_SIZE, response_template, 
                          status_code, status_text, 
                          strlen(status_text) + 4,
                          status_code, status_text);
        SSL_write(ssl, response, len);
    } else {
        // Serve custom HTML error page
        struct stat file_stat;
        fstat(file_fd, &file_stat);
        char headers[BUFFER_SIZE];
        snprintf(headers, BUFFER_SIZE, 
                "HTTP/1.1 %d %s\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: %lld\r\n\r\n", 
                status_code, status_text, file_stat.st_size);
        SSL_write(ssl, headers, strlen(headers));
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
            SSL_write(ssl, buffer, bytes_read);
        }
        close(file_fd);
    }
}

static void serve_directory_listing(SSL *ssl, const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        serve_error(ssl, 500, "Internal Server Error");
        return;
    }

    // Get the web path (remove "public" prefix)
    const char *web_path = path + 6;  // Skip "public" prefix
    if (strlen(web_path) == 0) web_path = "/";

    // Build HTML content with improved styling
    char response[BUFFER_SIZE * 4] = {0};  // Increased buffer size
    snprintf(response, BUFFER_SIZE * 4,
        "<html><head>"
        "<title>Index of %s</title>"
        "<style>"
        "body { font-family: Arial, sans-serif; margin: 40px; }"
        "h1 { margin-bottom: 20px; }"
        "ul { list-style-type: none; padding: 0; }"
        "li { margin: 10px 0; }"
        "a { text-decoration: none; color: #0066cc; }"
        "a:hover { text-decoration: underline; }"
        "</style>"
        "</head><body>"
        "<h1>Index of %s</h1>"
        "<ul>",
        web_path, web_path);

    // Add parent directory link if not at root
    if (strcmp(web_path, "/") != 0) {
        strcat(response, "<li><a href=\"..\">..</a></li>");
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check if it's a directory
        char full_entry_path[BUFFER_SIZE];
        snprintf(full_entry_path, BUFFER_SIZE, "%s/%s", path, entry->d_name);
        struct stat entry_stat;
        stat(full_entry_path, &entry_stat);
        
        char entry_html[BUFFER_SIZE];
        if (S_ISDIR(entry_stat.st_mode)) {
            snprintf(entry_html, BUFFER_SIZE,
                "<li><a href=\"%s/\">%s/</a></li>",
                entry->d_name, entry->d_name);
        } else {
            snprintf(entry_html, BUFFER_SIZE,
                "<li><a href=\"%s\">%s</a></li>",
                entry->d_name, entry->d_name);
        }
        strcat(response, entry_html);
    }

    strcat(response, "</ul></body></html>");
    closedir(dir);

    // Send response
    char headers[BUFFER_SIZE];
    snprintf(headers, BUFFER_SIZE,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n\r\n",
        strlen(response));
    SSL_write(ssl, headers, strlen(headers));
    SSL_write(ssl, response, strlen(response));
}

// Serve a file based on the sanitized path
static void serve_file(SSL *ssl, const char *path) {
    char full_path[BUFFER_SIZE];
    snprintf(full_path, BUFFER_SIZE, "public%s", path);

    // Check if path is a directory
    struct stat path_stat;
    if (stat(full_path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        // Look for index.html
        char index_path[BUFFER_SIZE];
        snprintf(index_path, BUFFER_SIZE, "%s/index.html", full_path);
        
        if (access(index_path, F_OK) != -1) {
            // index.html exists, serve it
            strncat(full_path, "/index.html", BUFFER_SIZE - strlen(full_path) - 1);
        } else {
            // No index.html, show directory listing
            serve_directory_listing(ssl, full_path);
            return;
        }
    }
    
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd == -1) {
        perror("open failed");
        if (errno == ENOENT) {
            serve_error(ssl, 404, "Not Found");
        } else {
            serve_error(ssl, 500, "Internal Server Error");
        }
        return;
    }

    // Get file size
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    // Send HTTP headers
    char headers[BUFFER_SIZE];
    const char *mime_type = get_mime_type(full_path);
    snprintf(headers, BUFFER_SIZE, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %lld\r\n"
        "Content-Type: %s\r\n"
        "\r\n", file_size, mime_type);
    SSL_write(ssl, headers, strlen(headers));

    // Send file content
    char file_buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(file_fd, file_buffer, BUFFER_SIZE)) > 0) {
        SSL_write(ssl, file_buffer, bytes_read);
    }

    close(file_fd);
}

void handle_client(SSL *ssl) {
    char buffer[BUFFER_SIZE] = {0};

    // Read request via SSL
    int bytes_read = SSL_read(ssl, buffer, BUFFER_SIZE-1);
    if (bytes_read <= 0) {
        ERR_print_errors_fp(stderr);
        return;
    }
    buffer[bytes_read] = '\0';

    // Parse
    char method[16] = {0};
    char path[BUFFER_SIZE] = {0};
    if (parse_request_line(buffer, method, path) != 0) {
        serve_error(ssl, 400, "Bad Request");
        return;
    }
    if (!is_path_safe(path)) {
        serve_error(ssl, 403, "Forbidden");
        return;
    }
    if (strcmp(method, "GET") != 0) {
        serve_error(ssl, 501, "Not Implemented");
        return;
    }

    serve_file(ssl, path);
    SSL_shutdown(ssl);
}

int main(void) {  // Added void
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // 1. Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configure socket to reuse address/port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 2. Bind to port 8080
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP
    address.sin_port = htons(PORT);       // Convert port to network byte order

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Listen for incoming connections
    if (listen(server_fd, 10) == -1) { // Backlog of 10 pending connections
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Set up signal handler for zombie process cleanup
    signal(SIGCHLD, SIG_IGN);

    init_openssl(); 
    
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_fd == -1) {
            perror("accept failed");
            continue;
        }

        // Create SSL object
        SSL *ssl = SSL_new(ssl_ctx);
        SSL_set_fd(ssl, client_fd);

        // Perform TLS handshake
        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            close(client_fd);
            SSL_free(ssl);
            continue;
        }
        
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            close(client_fd);
            continue;
        } else if (pid == 0) {  // Child process
            close(server_fd);  
            handle_client(ssl);
            SSL_free(ssl);
            exit(0);
          
        } else {  // Parent process
            close(client_fd);  
            SSL_free(ssl);
        }
    }

    cleanup_openssl();
    close(server_fd);
    return 0;
}
