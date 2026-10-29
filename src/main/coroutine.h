#ifndef COROUTINE_H
#define COROUTINE_H

#include <stdint.h>

#define CORO_BEGIN(VARS, ...) \
    *coro_status = CORO_STATUS_DONE; \
    struct _vars __VA_ARGS__ *VARS = (void *)coro_state->vars; \
    static_assert(sizeof(struct _vars) <= sizeof(coro_state->vars), \
                  "too many coroutine variables"); \
    switch (coro_state->suspend_point) { case 0:;
#define CORO_YIELD(RESULT) \
    *coro_status = CORO_STATUS_YIELD; \
    coro_state->suspend_point = __LINE__; return RESULT; case __LINE__:;
#define CORO_END(RESULT) \
    coro_state->suspend_point = -1; \
    return RESULT; }
#define CORO_FUNC(NAME, ...) \
    NAME(coro_status_e *coro_status, coro_state_s *coro_state, ##__VA_ARGS__)

typedef enum coro_status
{
    CORO_STATUS_YIELD,
    CORO_STATUS_DONE,
}
coro_status_e;

typedef struct coro_state
{
    int suspend_point;
    uintptr_t vars[32];
}
coro_state_s;

#endif