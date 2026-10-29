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