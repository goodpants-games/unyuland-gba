#ifndef PLATUTIL_H
#define PLATUTIL_H

// TODO: do i need to put long_call?
#define ARM_FUNC __attribute__((section(".iwram"), long_call, target("arm")))
#define NO_INLINE __attribute__((noinline))
#define PLATFORM_GBA

#endif