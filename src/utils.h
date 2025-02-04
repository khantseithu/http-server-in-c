#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

bool is_path_safe(const char *path);
const char *get_mime_type(const char *path);
void urldecode(const char *src, char *dest, size_t dest_size);

#endif // UTILS_H

