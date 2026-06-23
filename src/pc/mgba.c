#include <__stdarg_va_list.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <mgba.h>

bool mgba_open(void) { return true; }
void mgba_close(void) {}

void mgba_printf(int level, const char* string, ...)
{
    switch (level)
    {
    case MGBA_LOG_DEBUG:
        fprintf(stderr, "[ERR] ");
        break;

    case MGBA_LOG_INFO:
        fprintf(stderr, "[INF] ");
        break;

    case MGBA_LOG_WARN:
        fprintf(stderr, "[WRN] ");
        break;

    case MGBA_LOG_ERROR:
        fprintf(stderr, "[ERR] ");
        break;
    
    case MGBA_LOG_FATAL:
        fprintf(stderr, "[FTL] ");
        break;

    default:
        fprintf(stderr, "[???] ");
        break;
    }

    va_list va;
    va_start(va, string);
    vfprintf(stderr, string, va);
    va_end(va);

    fprintf(stderr, "\n");
}

bool mgba_console_open(void) { return true; }