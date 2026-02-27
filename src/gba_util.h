#ifndef GBA_UTIL_H
#define GBA_UTIL_H

// TODO: do i need to put long_call?
#define ARM_FUNC __attribute__((section(".iwram"), long_call, target("arm")))
#define NO_INLINE __attribute__((noinline))

#endif