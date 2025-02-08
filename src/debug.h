#ifdef DEBUG
#define DEBUG_PRINT(...) debug_printf(__VA_ARGS__)
void debug_printf(const char *format, ...);
#else
#define DEBUG_PRINT(...) // Do nothing
#endif
