#include "threads.h"


#ifdef DEBUG_MODE
#include <stdio.h>
int32_t threads_active = 0;
#endif


int32_t create_thread(  thread_t* thread, const thread_attr_t* attr,
                        void* (*start_routine)(void*), void* arg) {
    uint32_t creation_flags = 0;                        
    uint32_t ss_size = 0;
    
    if (attr != NULL) {
        if (attr->ss_size != 0) {
            ss_size = attr->ss_size;
            creation_flags |= STACK_SIZE_PARAM_IS_A_RESERVATION;
        }
        if (attr->suspended) {
            creation_flags |= CREATE_SUSPENDED;
        }
    }
    
    HANDLE thread_handle = CreateThread(
        NULL, // windows security options - unnessecery
        (DWORD)                  ss_size,
        (LPTHREAD_START_ROUTINE) start_routine,
        (LPVOID)                 arg,
        (DWORD)                  creation_flags,
        (PDWORD)                 &(thread->thread_id)
    );
    
    // eroor
    if (thread_handle == NULL) return GetLastError();

    thread->thread_handle = thread_handle;

    #ifdef DEBUG_MODE
    threads_active += 1; printf("threads += 1 (%d)\t: %d\n", thread->thread_id, threads_active);
    #endif
    return 0;
}

// returns the thread's previous suspend count
int32_t suspend_thread(thread_t thread) {
    return SuspendThread(thread.thread_handle);
}

// returns the thread's previous suspend count.
int32_t resume_thread(thread_t thread) {
    return ResumeThread(thread.thread_handle);
}

void exit_thread(int32_t exit_code) {
    #ifdef DEBUG_MODE
    threads_active -= 1; printf("threads -= 1 (%d)\t: %d\n", thread_self(), threads_active);
    #endif
    ExitThread(exit_code);
}

// returns the thread's exit code
int32_t join_thread(thread_t thread) {
    // wait
    WaitForSingleObject(
        thread.thread_handle,
        INFINITE // The time-out interval [...] If dwMilliseconds is INFINITE, the function will return only when the object is signaled.
    );
    
    #ifdef DEBUG_MODE
    if (get_thread_exit_code(thread) == STILL_ACTIVE) {threads_active -= 1; printf("threads -= 1 (%d)\t: %d\n", thread.thread_id, threads_active);}
    #endif
    
    // get exit code
    int32_t exit_code = get_thread_exit_code(thread);

    // close the handle
    CloseHandle(thread.thread_handle);

    return exit_code;
}

void terminate_thread(thread_t thread) {
    #ifdef DEBUG_MODE
    if (get_thread_exit_code(thread) == STILL_ACTIVE) {threads_active -= 1; printf("threads -= 1 (%d)\t: %d\n", thread.thread_id, threads_active);}
    #endif
    
    TerminateThread(thread.thread_handle, 0);
    
    // close the handle
    CloseHandle(thread.thread_handle);
}

// \returns The thread's exit code or `STILL_ACTIVE` if thread did not exit
int32_t get_thread_exit_code(thread_t thread) {
    int32_t exit_code;
    GetExitCodeThread(
        thread.thread_handle,
        (PDWORD)&exit_code
    );
    return exit_code;
}

// \returns The thread's exit code, `0x104` if doesnt exist or `STILL_ACTIVE` if thread did not exit.
int32_t get_thread_exit_code_from_id(int32_t thread_id) {
    HANDLE thread_handle = OpenThread(
        THREAD_QUERY_INFORMATION,
        FALSE,
        thread_id
    );

    if (thread_handle == NULL) return 0x104;

    int32_t exit_code;
    GetExitCodeThread(
        thread_handle,
        (PDWORD)&exit_code
    );

    CloseHandle(thread_handle);

    return exit_code;
}

// returns the current thread's id
int32_t thread_self() {
    return GetCurrentThreadId();
}

void sleep(uint32_t ms) {
    Sleep(ms);
}