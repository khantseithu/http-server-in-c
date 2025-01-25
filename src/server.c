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
#include <sys/wait.h>  // Add for waitpid()
#include <signal.h>    // Add for signal handling
#include <dirent.h>  // For directory operations

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

void serve_error(int client_fd, int status_code, const char *status_text) {
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
        send(client_fd, response, len, 0);
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
        send(client_fd, headers, strlen(headers), 0);

        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
            send(client_fd, buffer, bytes_read, 0);
        }
        close(file_fd);
    }
}

void serve_directory_listing(int client_fd, const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        serve_error(client_fd, 500, "Internal Server Error");
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
    send(client_fd, headers, strlen(headers), 0);
    send(client_fd, response, strlen(response), 0);
}

// Serve a file based on the sanitized path
void serve_file(int client_fd, const char *path) {
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
            serve_directory_listing(client_fd, full_path);
            return;
        }
    }
    
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd == -1) {
        perror("open failed");
        if (errno == ENOENT) {
            serve_error(client_fd, 404, "Not Found");
        } else {
            serve_error(client_fd, 500, "Internal Server Error");
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

    // Set up signal handler for zombie process cleanup
    signal(SIGCHLD, SIG_IGN);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_fd == -1) {
            perror("accept failed");
            continue;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            close(client_fd);
            continue;
        } else if (pid == 0) {  // Child process
            close(server_fd);    // Child doesn't need the listener socket

            // Read the client's request
            ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
            if (bytes_read < 0) {
                perror("read failed");
                close(client_fd);
                exit(EXIT_FAILURE);
            }
            buffer[bytes_read] = '\0';
            printf("Raw request:\n%s\n", buffer);

            // Parse the request line
            char method[16] = {0};
            char path[BUFFER_SIZE] = {0};
            if (parse_request_line(buffer, method, path) != 0) {
                char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 15\r\n\r\nInvalid request";
                send(client_fd, response, strlen(response), 0);
                close(client_fd);
                exit(EXIT_FAILURE);
            }

            // ...rest of request handling...
            if (!is_path_safe(path)) {
                char *response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 13\r\n\r\nInvalid path";
                send(client_fd, response, strlen(response), 0);
                close(client_fd);
                exit(EXIT_FAILURE);
            }

            if (strcmp(method, "GET") != 0) {
                char *response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 21\r\n\r\nMethod not supported";
                send(client_fd, response, strlen(response), 0);
                close(client_fd);
                exit(EXIT_FAILURE);
            }

            printf("[PID %d] Method: %s Path: %s\n", getpid(), method, path);
            serve_file(client_fd, path);
            close(client_fd);
            exit(EXIT_SUCCESS);
        } else {  // Parent process
            close(client_fd);  // Parent doesn't need the client socket
            // Zombie cleanup is handled by the signal handler
        }
    }

    close(server_fd);
    return 0;
}
