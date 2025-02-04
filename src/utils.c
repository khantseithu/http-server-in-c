#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

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

void urldecode(const char *src, char *dest, size_t dest_size) {
    char *p = dest;
    const char *end = dest + dest_size - 1; // Reserve space for null terminator

    while (*src && p < end) {
        if (*src == '%') {
            if (src + 2 < end && isxdigit(src[1]) && isxdigit(src[2])) {
                char hex[3] = { src[1], src[2], '\0' };
                *p++ = (char)strtol(hex, NULL, 16);
                src += 3;
            } else {
                // Invalid encoding, copy as is
                *p++ = *src++;
            }
        } else if (*src == '+') {
            *p++ = ' ';
            src++;
        } else {
            *p++ = *src++;
        }
    }
    *p = '\0'; // Null-terminate the destination string
}
