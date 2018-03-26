#ifndef V_MODULE_H
#define V_MODULE_H

#define V_SYS_API extern "C"

#ifdef _WIN
#include <windows.h>
typedef DWORD WINAPI (*ThreadProcFunc)(void *Parm);
typedef HANDLE ThreadHandle;
#else
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

typedef  unsigned int DWORD;
typedef  uint8_t  BYTE;
typedef void* (*ThreadProcFunc)(void *Parm);
typedef pthread_t ThreadHandle; 
#endif

/* Get Count Milliseconds */
inline long V_Milliseconds() {
#ifdef _WIN
    /*SYSTEMTIME time;
    GetSystemTime(&time);
    return (((wHour*60+time.wMinute)*60) + time.wSecond * 1000) + time.wMilliseconds;*/
    return (long)GetTickCount();
#else
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_sec*1000 + spec.tv_nsec/1.0e6;
#endif
}

/* Create Thread Function */
inline ThreadHandle V_CreateThread(ThreadProcFunc Func, void *arg) {
#ifdef _WIN
    DWORD dwThreadId;
    return (void *)CreateThread(NULL, 0, Func, arg, 0, &dwThreadId);
#else
    pthread_t r;
    pthread_create(&r, 0, Func, arg);
    return r;
#endif
}
/* Join Thread Function */
static int V_JoinThread(ThreadHandle &handle) {
#ifdef _WIN
    return WaitForSingleObject(handle, INFINITE);
#else
    return pthread_join(handle, NULL);
#endif
}

static void V_Sleep(long msec) {
#ifdef _WIN
    Sleep(msec);
#else
    struct timespec spec;
    spec.tv_sec = msec/1000;
    spec.tv_nsec = (msec%1000)*1000*1000;
    nanosleep(&spec, NULL);
#endif
}


#endif //v_module_h
