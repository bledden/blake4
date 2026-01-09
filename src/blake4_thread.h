/*
 * BLAKE4 Threading Abstraction Layer
 *
 * Platform-independent threading primitives for parallel hashing.
 * Supports pthreads (Unix/macOS) and Win32 threads (Windows).
 *
 * This is free and unencumbered software released into the public domain.
 */

#ifndef BLAKE4_THREAD_H
#define BLAKE4_THREAD_H

#include <stddef.h>

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
    #define BLAKE4_THREAD_WIN32 1
    #define BLAKE4_THREAD_AVAILABLE 1
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
    #define BLAKE4_THREAD_PTHREAD 1
    #define BLAKE4_THREAD_AVAILABLE 1
#else
    #define BLAKE4_THREAD_AVAILABLE 0
#endif

/* Platform-specific includes for CPU detection */
#if BLAKE4_THREAD_AVAILABLE
    #if defined(__APPLE__)
        #include <sys/sysctl.h>
    #endif
    #if !defined(_WIN32) && !defined(_WIN64)
        #include <unistd.h>
    #endif
#endif

#if BLAKE4_THREAD_AVAILABLE

#if BLAKE4_THREAD_WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>

    typedef HANDLE blake4_thread_t;
    typedef CRITICAL_SECTION blake4_mutex_t;
    typedef CONDITION_VARIABLE blake4_cond_t;
    typedef DWORD (WINAPI *blake4_thread_func_t)(LPVOID);

    #define BLAKE4_THREAD_FUNC_DECL(name, arg) DWORD WINAPI name(LPVOID arg)
    #define BLAKE4_THREAD_RETURN return 0

#elif BLAKE4_THREAD_PTHREAD
    #include <pthread.h>

    typedef pthread_t blake4_thread_t;
    typedef pthread_mutex_t blake4_mutex_t;
    typedef pthread_cond_t blake4_cond_t;
    typedef void *(*blake4_thread_func_t)(void *);

    #define BLAKE4_THREAD_FUNC_DECL(name, arg) void *name(void *arg)
    #define BLAKE4_THREAD_RETURN return NULL
#endif

/* Thread creation and joining */
static inline int blake4_thread_create(blake4_thread_t *thread,
                                        void *(*func)(void *),
                                        void *arg) {
#if BLAKE4_THREAD_WIN32
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    return (*thread != NULL) ? 0 : -1;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_create(thread, NULL, func, arg);
#endif
}

static inline int blake4_thread_join(blake4_thread_t thread) {
#if BLAKE4_THREAD_WIN32
    DWORD result = WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return (result == WAIT_OBJECT_0) ? 0 : -1;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_join(thread, NULL);
#endif
}

/* Mutex operations */
static inline int blake4_mutex_init(blake4_mutex_t *mutex) {
#if BLAKE4_THREAD_WIN32
    InitializeCriticalSection(mutex);
    return 0;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_mutex_init(mutex, NULL);
#endif
}

static inline int blake4_mutex_destroy(blake4_mutex_t *mutex) {
#if BLAKE4_THREAD_WIN32
    DeleteCriticalSection(mutex);
    return 0;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_mutex_destroy(mutex);
#endif
}

static inline int blake4_mutex_lock(blake4_mutex_t *mutex) {
#if BLAKE4_THREAD_WIN32
    EnterCriticalSection(mutex);
    return 0;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_mutex_lock(mutex);
#endif
}

static inline int blake4_mutex_unlock(blake4_mutex_t *mutex) {
#if BLAKE4_THREAD_WIN32
    LeaveCriticalSection(mutex);
    return 0;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_mutex_unlock(mutex);
#endif
}

/* Condition variable operations */
static inline int blake4_cond_init(blake4_cond_t *cond) {
#if BLAKE4_THREAD_WIN32
    InitializeConditionVariable(cond);
    return 0;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_cond_init(cond, NULL);
#endif
}

static inline int blake4_cond_destroy(blake4_cond_t *cond) {
#if BLAKE4_THREAD_WIN32
    /* Windows condition variables don't need explicit destruction */
    (void)cond;
    return 0;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_cond_destroy(cond);
#endif
}

static inline int blake4_cond_signal(blake4_cond_t *cond) {
#if BLAKE4_THREAD_WIN32
    WakeConditionVariable(cond);
    return 0;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_cond_signal(cond);
#endif
}

static inline int blake4_cond_broadcast(blake4_cond_t *cond) {
#if BLAKE4_THREAD_WIN32
    WakeAllConditionVariable(cond);
    return 0;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_cond_broadcast(cond);
#endif
}

static inline int blake4_cond_wait(blake4_cond_t *cond, blake4_mutex_t *mutex) {
#if BLAKE4_THREAD_WIN32
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
#elif BLAKE4_THREAD_PTHREAD
    return pthread_cond_wait(cond, mutex);
#endif
}

/* CPU count detection */
static inline unsigned blake4_cpu_count(void) {
#if BLAKE4_THREAD_WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (unsigned)sysinfo.dwNumberOfProcessors;
#elif BLAKE4_THREAD_PTHREAD
    #if defined(__APPLE__)
        int count;
        size_t size = sizeof(count);
        if (sysctlbyname("hw.ncpu", &count, &size, NULL, 0) == 0) {
            return (unsigned)count;
        }
        return 1;
    #elif defined(_SC_NPROCESSORS_ONLN)
        long count = sysconf(_SC_NPROCESSORS_ONLN);
        return (count > 0) ? (unsigned)count : 1;
    #else
        return 1;
    #endif
#else
    return 1;
#endif
}

#else /* !BLAKE4_THREAD_AVAILABLE */

/* Stub implementations when threading is not available */
typedef int blake4_thread_t;
typedef int blake4_mutex_t;
typedef int blake4_cond_t;

static inline int blake4_thread_create(blake4_thread_t *t, void *(*f)(void *), void *a) {
    (void)t; (void)f; (void)a; return -1;
}
static inline int blake4_thread_join(blake4_thread_t t) { (void)t; return -1; }
static inline int blake4_mutex_init(blake4_mutex_t *m) { (void)m; return 0; }
static inline int blake4_mutex_destroy(blake4_mutex_t *m) { (void)m; return 0; }
static inline int blake4_mutex_lock(blake4_mutex_t *m) { (void)m; return 0; }
static inline int blake4_mutex_unlock(blake4_mutex_t *m) { (void)m; return 0; }
static inline int blake4_cond_init(blake4_cond_t *c) { (void)c; return 0; }
static inline int blake4_cond_destroy(blake4_cond_t *c) { (void)c; return 0; }
static inline int blake4_cond_signal(blake4_cond_t *c) { (void)c; return 0; }
static inline int blake4_cond_broadcast(blake4_cond_t *c) { (void)c; return 0; }
static inline int blake4_cond_wait(blake4_cond_t *c, blake4_mutex_t *m) { (void)c; (void)m; return 0; }
static inline unsigned blake4_cpu_count(void) { return 1; }

#endif /* BLAKE4_THREAD_AVAILABLE */

#endif /* BLAKE4_THREAD_H */
