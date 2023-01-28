
#pragma once

#define COLOR_RED       "\x1B[31m"
#define COLOR_GREEN     "\x1B[32m"
#define COLOR_YELLOW    "\x1B[33m"
#define COLOR_BLUE      "\x1B[34m"
#define COLOR_MAGENTA   "\x1B[35m"
#define COLOR_CYAN      "\x1B[36m"
#define COLOR_WHITE     "\x1B[37m"
#define COLOR_RESET     "\x1B[0m"

#ifdef USE_PRINTFC
#define printfc(format, color, ...) printf("%s" format "%s", color, __VA_ARGS__, COLOR_RESET)
#define printc(text, color) printf("%s" text "%s", color, COLOR_RESET)
#else
#define printfc(format, color, ...) printf(format, __VA_ARGS__)
#define printc(text, color) printf(text)
#endif
