/*
Copyright (C) 2026  pkhead

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

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
        fprintf(stderr, "[DBG] ");
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