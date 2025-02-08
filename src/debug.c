#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>

void debug_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    pid_t pid = getpid();

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec;
    struct tm *t = localtime(&now);
    char time_str[100];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d.%03ld", t->tm_hour, t->tm_min, t->tm_sec, tv.tv_usec / 1000);

    printf("[%s] [PID: %d] ", time_str, pid);
    vprintf(format, args);

    va_end(args);
}
