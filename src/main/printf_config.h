// disable float support on a full-release build, since Well. The game shouldn't
// be using floats anyway. floats are only used debug logging displaying fixed-
// point values.
#ifndef DEVDEBUG
#define PRINTF_DISABLE_SUPPORT_FLOAT
#define PRINTF_DISABLE_SUPPORT_EXPONENTIAL

#endif

void _putchar(char character) {}
