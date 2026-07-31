#ifndef LOG_H
#define LOG_H

// DBG_CRASH and DBG_BREAK should be in plautil.h instead, but whatever...

#ifdef DEVDEBUG

#include <mgba.h>

#define LOG_INIT() mgba_console_open()

#define LOG_DBG(fmt, ...) mgba_printf(MGBA_LOG_DEBUG, fmt, ## __VA_ARGS__)
#define LOG_INF(fmt, ...) mgba_printf(MGBA_LOG_INFO, fmt, ## __VA_ARGS__)
#define LOG_WRN(fmt, ...) mgba_printf(MGBA_LOG_WARN, fmt, ## __VA_ARGS__)
#define LOG_ERR(fmt, ...) mgba_printf(MGBA_LOG_ERROR, fmt, ## __VA_ARGS__)
#define DBG_CRASH() __builtin_trap()

#if defined(PLATFORM_GBA)
#   define DBG_BREAK() asm volatile ("mov r11, r11")
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined (_M_IX86)
#   define DBG_BREAK() asm volatile ("int $0x03")
#elif defined(__arm__) || defined(_M_ARM)
#   define DBG_BREAK() arm volatile (".inst 0xe7f001f0")
#elif defined(__aarch64__) || defined(_M_ARM64)
#   define DBG_BREAK() asm volatile (".inst 0xd4200000")
#else
#   define DBG_BREAK() DBG_CRASH()
#endif

#else

#define LOG_INIT()
#define LOG_DBG(fmt, ...)
#define LOG_INF(fmt, ...)
#define LOG_WRN(fmt, ...)
#define LOG_ERR(fmt, ...)
#define DBG_CRASH()
#define DBG_BREAK()

#endif

#endif