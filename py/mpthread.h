/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George on behalf of Pycom Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef MICROPY_INCLUDED_PY_MPTHREAD_H
#define MICROPY_INCLUDED_PY_MPTHREAD_H

#include "py/mpconfig.h"

#if MICROPY_PY_THREAD

struct _mp_state_thread_t;

#ifdef MICROPY_MPTHREADPORT_H
#include MICROPY_MPTHREADPORT_H
#else
#include <mpthreadport.h>
#endif

struct _mp_state_thread_t *mp_thread_get_state(void);
void mp_thread_set_state(struct _mp_state_thread_t *state);
mp_uint_t mp_thread_create(void *(*entry)(void *), void *arg, size_t *stack_size);
mp_uint_t mp_thread_get_id(void);
void mp_thread_start(void);
void mp_thread_finish(void);
void mp_thread_mutex_init(mp_thread_mutex_t *mutex);
int mp_thread_mutex_lock(mp_thread_mutex_t *mutex, int wait);
void mp_thread_mutex_unlock(mp_thread_mutex_t *mutex);

#if MICROPY_PY_THREAD_RECURSIVE_MUTEX
void mp_thread_recursive_mutex_init(mp_thread_recursive_mutex_t *mutex);
int mp_thread_recursive_mutex_lock(mp_thread_recursive_mutex_t *mutex, int wait);
void mp_thread_recursive_mutex_unlock(mp_thread_recursive_mutex_t *mutex);
#endif

#endif // MICROPY_PY_THREAD

#if MICROPY_PY_THREAD && MICROPY_PY_THREAD_GIL
#include "py/mpstate.h"

typedef void (*pm_metal_async_gil_on_release_fn)(void);

#if MICROPY_PY_METAL

/* CAS-GIL: atomic owner+count replaces the pthread mutex.
 * No OS thread ever blocks — REPL thread spin-polls pm_metal_async_gil_poll()
 * on contention, servicing the async runner while waiting.
 * Recursive re-entry: same thread bumps count, no deadlock.
 *
 * Everything is in macro statement-expressions. mpthread.h is included from
 * mpstate.h before mp_state_vm_t is defined, so inline functions referencing
 * MP_STATE_VM would not compile — only macros expanded at the call site work. */

typedef void (*pm_metal_async_gil_poll_fn)(void);
extern pm_metal_async_gil_poll_fn pm_metal_async_gil_poll;

/* Private: non-blocking CAS acquire. Returns 1 on success, 0 on contention. */
#define _MP_GIL_CAS_TRY() ({ \
    int _ok = 0; \
    mp_uint_t _tid = mp_thread_get_id(); \
    if (MP_STATE_VM(gil_owner) == _tid) { \
        MP_STATE_VM(gil_count) += 1; \
        _ok = 1; \
    } else if (MP_STATE_VM(gil_owner) == 0 && \
               __sync_bool_compare_and_swap(&MP_STATE_VM(gil_owner), 0, _tid)) { \
        MP_STATE_VM(gil_count) = 1; \
        _ok = 1; \
    } \
    _ok; \
})

/* Private: blocking CAS acquire (never returns on metal seats). */
#define _MP_GIL_CAS_ENTER() do { \
    int _got = _MP_GIL_CAS_TRY(); \
    if ((_got)) { break; } \
    while (!(_got)) { \
        if (pm_metal_async_gil_poll != NULL) { pm_metal_async_gil_poll(); } \
        _got = _MP_GIL_CAS_TRY(); \
    } \
} while (0)

/* Private: release the GIL. Recursive: decrements count, unlocks only at 0. */
#define _MP_GIL_CAS_EXIT() do { \
    if (MP_STATE_VM(gil_count) > 1) { \
        MP_STATE_VM(gil_count) -= 1; \
    } else { \
        MP_STATE_VM(gil_count) = 0; \
        __sync_synchronize(); \
        MP_STATE_VM(gil_owner) = 0; \
    } \
} while (0)

#define MP_THREAD_GIL_ENTER() _MP_GIL_CAS_ENTER()
#define MP_THREAD_GIL_EXIT() do { \
    _MP_GIL_CAS_EXIT(); \
    if (pm_metal_async_gil_on_release != NULL) { pm_metal_async_gil_on_release(); } \
} while (0)
#define MP_THREAD_GIL_TRYLOCK() _MP_GIL_CAS_TRY()

#else /* !MICROPY_PY_METAL — original pthread GIL */

#if MICROPY_PY_THREAD_RECURSIVE_MUTEX
#define MP_THREAD_GIL_ENTER() mp_thread_recursive_mutex_lock(&MP_STATE_VM(gil_mutex), 1)
#define MP_THREAD_GIL_EXIT() do { \
    mp_thread_recursive_mutex_unlock(&MP_STATE_VM(gil_mutex)); \
    if (pm_metal_async_gil_on_release != NULL) { pm_metal_async_gil_on_release(); } \
} while (0)
#define MP_THREAD_GIL_TRYLOCK() mp_thread_recursive_mutex_lock(&MP_STATE_VM(gil_mutex), 0)
#else
#define MP_THREAD_GIL_ENTER() mp_thread_mutex_lock(&MP_STATE_VM(gil_mutex), 1)
#define MP_THREAD_GIL_EXIT() do { \
    mp_thread_mutex_unlock(&MP_STATE_VM(gil_mutex)); \
    if (pm_metal_async_gil_on_release != NULL) { pm_metal_async_gil_on_release(); } \
} while (0)
#define MP_THREAD_GIL_TRYLOCK() mp_thread_mutex_lock(&MP_STATE_VM(gil_mutex), 0)
#endif

#endif /* MICROPY_PY_METAL */

extern pm_metal_async_gil_on_release_fn pm_metal_async_gil_on_release;

#else
#define MP_THREAD_GIL_ENTER()
#define MP_THREAD_GIL_EXIT()
#define MP_THREAD_GIL_TRYLOCK() 1
#endif

#endif // MICROPY_INCLUDED_PY_MPTHREAD_H
