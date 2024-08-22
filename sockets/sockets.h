#ifndef SOCKETS_h
#define SOCKETS_h

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>

#include "../threads/threads.h"


#ifdef DEUBUG_MODE
#define DEBUG_SOFT_MODE
#endif


#define _SOCKETS_ACCEPT_DEFAULT_BACKLOG_ (8)

#ifndef _SOCKETS_MAX_AMOUNT_
#define _SOCKETS_MAX_AMOUNT_ (128)
#endif



typedef struct socket_t {
    const uint32_t socket_index;

    SOCKET sock;
    struct sockaddr addr;

    // for listening-and-accepting sockets
    uint8_t accepting_sockets;
    thread_t accept_thread;
    void (*handler)(struct socket_t*);

    // for accepted sockets
    uint8_t accepted_socket;
    thread_t handler_thread;

    // for connected sockets (and accepted sockets)
    uint8_t connected_socket;
} socket_t;


extern uint32_t sockets_amount;
extern socket_t* sockets_list[];


/* \brief Initiallise the sockets library.
 * 
 * must be called before any other sockets function.
 * 
 * \returns 0 on success or error code.
 */
int32_t init_sockets();


/* \brief Creates an AF_INTET socket with TCP protocol.
 * 
 * The socket should be destroyed when irelevent by calling `destroy_socket(socket_t*)` or `clean_sockets()`.
 * 
 * \param ipaddr The socket's IPv4 address.
 * 
 * \param port The socket's port.
 * 
 * \returns A pointer to an allocated socket_t or `NULL` on failure.
 */
socket_t* create_socket_ext(uint32_t ipaddr, uint16_t port);

/* \brief Creates an AF_INTET socket with TCP protocol on a IPv4 local address.
 * 
 * The socket should be destroyed when irelevent by calling `destroy_socket(socket_t*)` or `clean_sockets()`.
 * 
 * \param port The socket's port.
 * 
 * \returns A pointer to an allocated socket_t or `NULL` on failure.
 */
socket_t* create_socket_local(uint16_t port);

/* \brief Creates an AF_INTET socket with TCP protocol.
 * 
 * The socket should be destroyed when irelevent by calling `destroy_socket(socket_t*)` or `clean_sockets()`.
 * 
 * \param ipv4addr The socket's IPv4 dotted-decimal address string.
 * 
 * \param port The socket's port.
 * 
 * \returns A pointer to an allocated socket_t or `NULL` on failure.
 */
socket_t* create_socket(const char* ipv4addr, uint16_t port);

/* \brief Creates an AF_INTET socket with TCP protocol that is connected to ipv4 `ipv4addr` and port `port`.
 * 
 * The socket should be destroyed when irelevent by calling `destroy_socket(socket_t*)` or `clean_sockets()`.
 * 
 * \param ipv4addr The IPv4 dotted-decimal address string to connect to.
 * 
 * \param port The port to connect to.
 * 
 * \returns A pointer to an allocated socket_t or `NULL` on failure.
 */
socket_t* create_connected_socket(const char* ipv4addr, uint16_t port);

/* \brief The socket will start listening for incoming connections and will accept these connections.
 * 
 * The acceptenace of connections will run asynchronously on a new thread.
 * 
 * \param sock The socket that will be listening.
 * 
 * \param backlog The maximum amount of connections pending acceptance.
 * 
 * \param handler A pointer to a function that will handle accepted sockets.
 * The function is passed the newly connected socket.
 * This function will run asynchronously on a new thread.
 * Calling `destroy_socket` from the handler will destroy the socket but not the handler's thread
 * so it is safe to do so.
 * You should call `exit_thread` after destroying the socket.
 * 
 * \returns 0 on success or 1 on error.
 */
int32_t listen_and_accept_sockets_ext(socket_t* sock, int32_t backlog, void (*handler)(socket_t*));

/* \brief The socket will start listening for incoming connections and will accept these connections.
 * 
 * The acceptenace of connections will run asynchronously on a new thread.
 * 
 * The maximum amount of connections pending acceptance will be `_SOCKETS_ACCEPT_DEFAULT_BACKLOG_`.
 * 
 * \param sock The socket that will be listening.
 * 
 * \param handler A pointer to a function that will handle accepted sockets.
 * The function is passed the newly connected socket.
 * This function will run asynchronously on a new thread.
 * Calling `destroy_socket` from the handler will destroy the socket but not the handler's thread
 * so it is safe to do so.
 * You should call `exit_thread` after destroying the socket.
 *
 * \returns 0 on success or 1 on error.
 */
int32_t listen_and_accept_sockets(socket_t* sock, void (*handler)(socket_t*));


/* \brief Send data (a buffer) to a socket.
 * 
 * If the buffer is too long it may not be sent in its entirety.
 * 
 * \param sock The socket to send the buffer to.
 * 
 * \param buffer The buffer to send.
 * 
 * \param buffer_length The length of the buffer in bytes.
 * 
 * \returns The number of bytes of the buffer sent, Or `SOCKET_ERROR` if an error occoured.
 */
int32_t send_to_socket(socket_t* sock, char* buffer, int32_t buffer_length);

/* \brief Receives data (a buffer) from a socket.
 * 
 * If the buffer is too short it may not get the entire data sent.
 * 
 * If the data sent is too short it may not fill the buffer completly 
 * 
 * \param sock The socket to receive the buffer from.
 * 
 * \param buffer A pointer to a buffer that will be filled with the received data.
 * 
 * \param buffer_length The size of the buffer in bytes, The max amount of bytes to receive.
 * 
 * \returns The number of bytes received, `0` if the connection was gracefully closed, Or `SOCKET_ERROR` if an error occoured.
 */
int32_t receive_from_socket(socket_t* sock, char* buffer, int32_t buffer_length);


/* \returns The IP address of the socket.
 */
int32_t get_socket_ip(socket_t* sock);

/* \returns The number of seconds a socket has been connected or `SOCKET_ERROR` on failure.
 */
int32_t get_socket_connect_time(socket_t* sock);

/* \returns The timeout, in milliseconds, for blocking receive calls (`receive_from_socket`), Or `SOCKET_ERROR` on failure.
 * 
 * The default timeout is zero, which indicates that a receive operation will not time out.
 */
int32_t get_socket_recieve_time_out(socket_t* sock);

/* \brief Sets the timeout, in milliseconds, for blocking receive calls (`receive_from_socket`).
 * 
 * The default timeout is zero, which indicates that a receive operation will not time out.
 * 
 * \returns `0` If no error occurs, Otherwise, a value of `SOCKET_ERROR` is returned.
 */
int32_t set_socket_recieve_time_out(socket_t* sock, int32_t timeout);

/* \brief Convert a uint32_t to a IPv4 dotted-decimal address string representing it.
 *
 * \param str_buf A char buffer the string will be written to. Minimum length 16.
 * 
 * \param ipaddr The uint32_t ip address.
 * 
 * \returns The length of the written string.
 */
int32_t ipstring_from_ipuint(char* str_buf, uint32_t ipaddr);


/* \brief Destroys a socket.
 *
 * The socket will not be usable after being destroyed.
 * 
 * If the socket was accepted by another socket, The `handler`'s thread of the accepted socket will be terminated.
 * This thread will be terminated before everything else will be freed, But, the thread will not be terminated if it
 * is the thread who called the funcion so it is safe to call from the `handler`'s thread.
 * 
 * If the socket was listening to accept a connection, The listening thread will be terminated unless it is the thread
 * who made the call to the function.
 *
 * \param sock the socket to clean.
 * 
 * \returns 1 if skipped terminating the current thread.
 */
int32_t destroy_socket(socket_t* sock);


/* Should be called before exiting.
 * Destroys all created sockets.
 *
 * \returns 1 if skipped terminating the current thread.
 */
int32_t clean_sockets();

/* \brief Prints a socket_t struct
 * 
 * \param sock The socket to print the data of
 * 
 * \param indentation A string that will be printed at the beggining of every line.
 */ 
void print_socket(socket_t* sock, const char* indentation);


#endif