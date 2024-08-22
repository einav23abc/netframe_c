#ifndef THREADS_H
#define THREADS_H

#include <stdint.h>
#include <windows.h>


typedef struct {
    int32_t thread_id;
    HANDLE thread_handle;
} thread_t;

typedef struct {
    uint8_t suspended; // start in suspended state
    uint32_t ss_size; // size of dynamically allocated stack
} thread_attr_t;



/* \brief Create a new thread that will run `start_routine`.
 * 
 * \param thread A pointer to a `thread_t` where the thread's data will be stored.
 * 
 * \param attr A pointer to a `thread_attr_t` which dictates the thread's attributes. Or `NULL` for no attributes.
 * 
 * \param start_routine A pointer to a function that the thread will run.
 * 
 * \param arg A pointer to the `start_routine`'s argument.
 * 
 * \returns 0 on success or an error code.
 */
int32_t create_thread(  thread_t* thread, const thread_attr_t* attr,
                        void* (*start_routine)(void*), void* arg);

/* \brief Pauses a thread's execution.
 *
 * \returns The thread's previous suspend count
 */
int32_t suspend_thread(thread_t thread);

/* \brief Unpauses a thread's execution. 
 *
 * \returns The thread's previous suspend count.
 */
int32_t resume_thread(thread_t thread);

/* \brief Closes the current thread and sets the exit code.
 */
void exit_thread(int32_t exit_code);

/* \brief Halts the current thread's execution until `thread` exits. 
 *
 * \returns `thread`'s exit code.
 */
int32_t join_thread(thread_t thread);

void terminate_thread(thread_t thread);

/* \returns The thread's exit code or `STILL_ACTIVE` if thread did not exit.
 */
int32_t get_thread_exit_code(thread_t thread);

/* \returns The thread's exit code, `0x104` if doesnt exist or `STILL_ACTIVE` if thread did not exit.
 */
int32_t get_thread_exit_code_from_id(int32_t thread_id);

/* \returns The current thread's id.
 */
int32_t thread_self();

/* \brief Halt the current thread's execution for `ms` milisecconds.
*/
void sleep(uint32_t ms);

#endif