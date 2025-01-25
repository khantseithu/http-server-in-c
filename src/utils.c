
#include <string.h>
#include <stdbool.h>

// Check if path is safe (no directory traversal)
bool is_path_safe(const char *path) {
    // Reject empty path
    if (strlen(path) == 0) return false;

    // Reject paths containing ".."
    if (strstr(path, "..") != NULL) return false;

    // Ensure path starts with '/'
    return path[0] == '/';
}

const char *get_mime_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";

    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "text/javascript";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
    return "application/octet-stream";
}