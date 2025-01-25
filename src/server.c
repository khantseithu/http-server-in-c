#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h> 
#include <sys/stat.h> // Added for stat()
#include <fcntl.h>    // Added for open(), read()
#include <errno.h>    // Added for errno

#define PORT 8080
#define BUFFER_SIZE 4096

int is_path_safe(const char *path) {
    // Check for path traversal attempts
    if (strstr(path, "..") != NULL) {
        return 0;
    }
    // Ensure path starts with /
    if (path[0] != '/') {
        return 0;
    }
    return 1;
}

// Helper function to parse the HTTP request line (e.g., "GET / HTTP/1.1")
int parse_request_line(const char *request_line, char *method, char *path) {
    // Copy the request line to avoid modifying the original buffer
    char line_copy[BUFFER_SIZE];
    strncpy(line_copy, request_line, BUFFER_SIZE);
    line_copy[BUFFER_SIZE - 1] = '\0'; // Ensure null-termination

    // Split the line into method, path, and HTTP version
    char *token = strtok(line_copy, " ");
    if (token == NULL) return -1; // Malformed
    strcpy(method, token);

    token = strtok(NULL, " ");
    if (token == NULL) return -1;
    strcpy(path, token);

    // Optional: Validate HTTP version (we'll ignore for now)
    return 0;
}

const char* get_mime_type(const char *file_path) {
    const char *dot = strrchr(file_path, '.');
    if (!dot) return "text/plain";
    
    if (strcasecmp(dot, ".html") == 0) return "text/html";
    if (strcasecmp(dot, ".css") == 0) return "text/css";
    if (strcasecmp(dot, ".js") == 0) return "application/javascript";
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(dot, ".png") == 0) return "image/png";
    if (strcasecmp(dot, ".gif") == 0) return "image/gif";
    
    return "text/plain";
}

// Serve a file based on the sanitized path
void serve_file(int client_fd, const char *path) {
    char full_path[BUFFER_SIZE];
    snprintf(full_path, BUFFER_SIZE, "public%s", path);

    // Check if path is a directory (append index.html)
    struct stat path_stat;
    if (stat(full_path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        strncat(full_path, "/index.html", BUFFER_SIZE - strlen(full_path) - 1);
    }

    // Open the file
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd == -1) {
        perror("open failed");
        if (errno == ENOENT) {
            // File not found: 404
            char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
            send(client_fd, response, strlen(response), 0);
        } else {
            // Other errors: 500
            char *response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 21\r\n\r\n500 Server Error";
            send(client_fd, response, strlen(response), 0);
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
    send(client_fd, headers, strlen(headers), 0);

    // Send file content
    char file_buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(file_fd, file_buffer, BUFFER_SIZE)) > 0) {
        send(client_fd, file_buffer, bytes_read, 0);
    }

    close(file_fd);
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

    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) == -1) {
            perror("accept failed");
            continue;
        }

        // Read the client's request
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read < 0) {
            perror("read failed");
            close(client_fd);
            continue;
        }
        buffer[bytes_read] = '\0'; // Null-terminate the request data
        printf("Raw request:\n%s\n", buffer);

        // Parse the request line
        char method[16] = {0}; // e.g., "GET"
        char path[BUFFER_SIZE] = {0}; // e.g., "/index.html"
        if (parse_request_line(buffer, method, path) != 0) {
            // Send "400 Bad Request" if parsing fails
            char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 15\r\n\r\nInvalid request";
            send(client_fd, response, strlen(response), 0);
            close(client_fd);
            continue;
        }

        if (!is_path_safe(path)) {
            char *response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 13\r\n\r\nInvalid path";
            send(client_fd, response, strlen(response), 0);
            close(client_fd);
            continue;
        }

        // Validate HTTP method (only GET is supported for now)
        if (strcmp(method, "GET") != 0) {
            char *response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 21\r\n\r\nMethod not supported";
            send(client_fd, response, strlen(response), 0);
            close(client_fd);
            continue;
        }

        // Log parsed values
        printf("Method: %s\nPath: %s\n", method, path);

        // Serve files based on path
        serve_file(client_fd, path);

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
