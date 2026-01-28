#ifndef LOG_H
#define LOG_H

#include "mgba.h"

#define LOG_INIT() mgba_console_open()

#define LOG_DBG(fmt, ...) mgba_printf(MGBA_LOG_DEBUG, fmt, ## __VA_ARGS__)
#define LOG_INF(fmt, ...) mgba_printf(MGBA_LOG_INFO, fmt, ## __VA_ARGS__)
#define LOG_WRN(fmt, ...) mgba_printf(MGBA_LOG_WARN, fmt, ## __VA_ARGS__)
#define LOG_ERR(fmt, ...) mgba_printf(MGBA_LOG_ERROR, fmt, ## __VA_ARGS__)

#endif