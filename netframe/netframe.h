#ifndef NETFRAME_h
#define NETFRAME_h

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../sockets/sockets.h"
#include "../threads/threads.h"


#ifndef _SERVER_OUT_PORT_
#define _SERVER_OUT_PORT_ (5050)
#endif
#ifndef _SERVER_IN_PORT_
#define _SERVER_IN_PORT_ (5051)
#endif

#ifndef _PACKET_MAX_LENGTH_
#define _PACKET_MAX_LENGTH_ (2048)
#endif
#ifndef _PACKETS_STACK_LENGTH_
#define _PACKETS_STACK_LENGTH_ (8)
#endif

#ifndef _CLIENTS_MAX_AMOUNT_
#define _CLIENTS_MAX_AMOUNT_ (10)
#endif


enum PACKET_TYPE {
    /* States that an error ocoured when receiving a packet.
     *
     * An error code `0` means: Connection with socket ended.
     * 
     * An error code `1` means: Received a packet bigger then `_PACKET_MAX_LENGTH_`.
     * 
     * An error code `SOCKET_ERROR` means: Buffer from socket did not arrive.
     * 
     * \param packet_type `RECV_ERROR`.
     * \param client_id Always `-1`.
     * \param packet_body[0] The recv error code.
     */
    RECV_ERROR,

    /* Sent from the `client-out-socket`.
     *
     * Completes the connection.
     * 
     * \param packet_type `CLIENT_OUT_SOCKET_CONNECT`.
     * \param client_id The id of the client that sent the packet
     * \param packet_body The `client ID` as `int8_t` that was issued by `SERVER_ASSIGNED_CLIENT_ID` to the `client-in-socket`.
     */
    CLIENT_OUT_SOCKET_CONNECT,
    /* Sent from the `client-out-socket`.
     * 
     * \param packet_type `CLIENT_UPDATE`.
     * \param client_id The id of the client that sent the packet
     * \param packet_body The update.
     */
    CLIENT_UPDATE,
    
    /* Sent to the `client-in-socket`.
     *
     * Should be passed by the client to the `client-out-socket` for a `CLIENT_OUT_SOCKET_CONNECT` packet.
     *
     * \param packet_type `SERVER_ASSIGNED_CLIENT_ID`.
     * \param client_id The newly connected client's assigned `client ID`.
     * \param packet_body Empty.
     */
    SERVER_ASSIGNED_CLIENT_ID,
    /* Sent to the `client-in-socket`.
     *
     * The server cannot assign a client ID.
     * 
     * The connection is refused.
     * 
     * \param packet_type `SERVER_NO_CLIENT_IDS`.
     * \param client_id `-1`.
     * \param packet_body Empty.
     */
    SERVER_NO_CLIENT_IDS,
    /* Sent to the `client-in-socket`
     *
     * A packet containing data for a reconstruction of the shared object on the client side.
     * 
     * A packet created by `gen_state_packet()` and should be parsed by `parse_state_packet()`, both user defined.
     * 
     * \param packet_type `SERVER_STATE`.
     * \param client_id `-1`.
     * \param packet_body Data set by `gen_state_packet()`.
     */
    SERVER_STATE,
    /* Sent to all `client-in-sockets`.
     *
     * A client X has connected.
     *
     * \param packet_type `SERVER_CLIENT_CONNECTED`.
     * \param client_id The assigned `client ID` of the connected client.
     * \param packet_body Empty.
     */
    SERVER_CLIENT_CONNECTED,
    /* Sent to all `client-in-sockets`.
     *
     * A client X has disconnected.
     *
     * \param packet_type `SERVER_CLIENT_DISCONNECTED`.
     * \param client_id The `client ID` of the disconnected client.
     * \param packet_body Empty.
     */
    SERVER_CLIENT_DISCONNECTED,
    /* Sent to all `client-in-sockets`.
     *
     * A client X has sent an update Y.
     *
     * \param packet_type `SERVER_CLIENT_UPDATE`.
     * \param client_id The `client ID` of the client who sent the update.
     * \param packet_body The update.
     */
    SERVER_CLIENT_UPDATE,
};

#pragma pack(1)
typedef struct {
    int32_t packet_len;

    union {
        char packet[_PACKET_MAX_LENGTH_];
        struct {
            uint8_t packet_type;
            int8_t client_id; // the client id refrencing
            char packet_body[_PACKET_MAX_LENGTH_-2];
        };
    };
} nf_packet_t;
#pragma pack()


/* Opens a server that listens and accept clients that connect with `join_server()`.
 * 
 * The server should be closed with `close_server()`
 * 
 * Upon client connections the server will call to `generate_state_packet()` if it exists, And will send the
 * generated packet to the newly connected client, which will call to `parse_state_packet()` if it exists.
 * The state packet should contain all the data to reconstruct the shared object at the client, this will be
 * done by the `parse_state_packet()` function that is defined by the framework's user. The `generate_state_packet()` should
 * also be defined by the framework's user.
 * 
 * Clients (and the server) can send updates to their data by calling `send_update_packet()`. The server will
 * send the update packet to all clients (including itself) which will call `parse_update_packet()` if it exists.
 * The update packet should contail all the data to convert the clients' shared object to the server's object, this
 * will be done by the `parse_update_packet()` function that is defined by the framework's user.
 * 
 * When a client connects/disconnects the server will tell all clients which will call to `handle_client_connect()` or
 * `handle_client_disconnect()` or `handle_disconnect_as_client()` all defined by the framework's user.
 * 
 * \param ipv4addr The server's IPv4 dotted-decimal address string.
 * 
 * \returns 0 on success or 1 on failure.
 */
int32_t open_server(const char* ipv4addr);

/* Opens a server that listens and accept clients that connect with `join_server()` at a local ipv4 address.
 * 
 * The server should be closed with `close_server()`
 * 
 * Upon client connections the server will call to `generate_state_packet()` if it exists, And will send the
 * generated packet to the newly connected client, which will call to `parse_state_packet()` if it exists.
 * The state packet should contain all the data to reconstruct the shared object at the client, this will be
 * done by the `parse_state_packet()` function that is defined by the framework's user. The `generate_state_packet()` should
 * also be defined by the framework's user.
 * 
 * Clients (and the server) can send updates to their data by calling `send_update_packet()`. The server will
 * send the update packet to all clients (including itself) which will call `parse_update_packet()` if it exists.
 * The update packet should contail all the data to convert the clients' shared object to the server's object, this
 * will be done by the `parse_update_packet()` function that is defined by the framework's user.
 * 
 * When a client connects/disconnects the server will tell all clients which will call to `handle_client_connect()` or
 * `handle_client_disconnect()` or `handle_disconnect_as_client()` all defined by the framework's user.
 * 
 * \returns 0 on success or 1 on failure.
 */
int32_t open_server_local();

/* \returns The IP of the opened server.
 */
uint32_t get_server_ip();

/* Closes a server opened by `open_server()` or `open_server_local()`.
 *
 * All connected clients will be disconnected.
 */
void close_server();

/* Join a server as a client.
 *
 * The client can exit the server by calling `exit_server()`.
 * 
 * Upon client connections the server will call to `generate_state_packet()` if it exists, And will send the
 * generated packet to the newly connected client, which will call to `parse_state_packet()` if it exists.
 * The state packet should contain all the data to reconstruct the shared object at the client, this will be
 * done by the `parse_state_packet()` function that is defined by the framework's user. The `generate_state_packet()` should
 * also be defined by the framework's user.
 * 
 * Clients (and the server) can send updates to their data by calling `send_update_packet()`. The server will
 * send the update packet to all clients (including itself) which will call `parse_update_packet()` if it exists.
 * The update packet should contail all the data to convert the clients' shared object to the server's object, this
 * will be done by the `parse_update_packet()` function that is defined by the framework's user.
 * 
 * When a client connects/disconnects the server will tell all clients which will call to `handle_client_connect()` or
 * `handle_client_disconnect()` or `handle_disconnect_as_client()` all defined by the framework's user.
 *
 * \param ipv4addr The server's IPv4 dotted-decimal address string.
 * 
 * \returns The assigned `client_id` on success or `-1` on failure.
 */
int32_t join_server(const char* ipv4addr);

/* Exit a server as a connected client.
 */
void exit_server();

/* Send an update packet to the server as a client.
 *
 * The server will send the update packet to all clients which will call `parse_update_packet()` if it exists.
 * The server will also call `parse_update_packet()` to allow it to act as a client too. The update packet should
 * contail all the data to convert the clients' shared object to the server's object, this will be done by
 * the `parse_update_packet()` function that is defined by the framework's user.
 * 
 * \param packet A nf_packet_t created by the client. The packet should have a type of `CLIENT_UPDATE` and
 * should be parsable by `parse_update_packet()` (defined by the framework's user).
 */
void send_update_packet(nf_packet_t packet);


/* Send an update packet to the server from the server pretending to be a client with id `-1`.
 *
 * The server will send the update packet to all clients which will call `parse_update_packet()` if it exists.
 * The server will also call `parse_update_packet()` to allow it to act as a client too. The update packet should
 * contail all the data to convert the clients' shared object to the server's object, this will be done by
 * the `parse_update_packet()` function that is defined by the framework's user.
 * 
 * \param packet A nf_packet_t created by the server. The packet should have a type of `CLIENT_UPDATE` and
 * should be parsable by `parse_update_packet()` (defined by the framework's user).
 */
void send_update_packet_as_server(nf_packet_t packet);


// user defined functions:

/* \brief Generate a packet that will be sent from the server to a client upon connection.
 *
 * The server will only send a state-packet if this function was defined by the framework's user.
 * 
 * The packet's `packet_type` must be `SERVER_STATE`.
 * 
 * The packet's `client_id` should generally be `-1`.
 * 
 * This packet will be parsed by `parse_state_packet` if it was defined by the framework's user.
 * 
 * \returns A state-packet of type `nf_packet_t`.
 */
__attribute__((weak)) nf_packet_t generate_state_packet();

/* \brief Parses a state-packet that was sent by the server.
 * 
 * The function will only be called it was defined by the framework's user.
 * 
 * The server will only send a state-packet if the `generate_state_packet` function was defined by the framework's user.
 * 
 * \param packet The state-packet.
 */
__attribute__((weak)) void parse_state_packet(nf_packet_t packet);
/* \brief Parse an update-packet that was sent by one of the clients (or the server) connected to the server.
 * 
 * The function will only be called it was defined by the framework's user.
 *
 * This packet can be sent by calling `send_update_packet` or `send_update_packet_as_server`.
 * 
 * \param packet The update-packet.
 */
__attribute__((weak)) void parse_update_packet(nf_packet_t packet);
/* \brief Handle a connection of a new client to the server you are connected to.
 *
 * The function will only be called it was defined by the framework's user.
 * 
 * \param client_id The assigned `client_id` of the newly connected client.
 */
__attribute__((weak)) void handle_client_connect(int32_t client_id);
/* \brief Handle client disconnected from the server you are connected to.
 *
 * The function will only be called it was defined by the framework's user.
 * 
 * \param client_id The `client_id` of the disconnected client.
 */
__attribute__((weak)) void handle_client_disconnect(int32_t client_id);
/* \brief You were disconnected from the server you were connected to.
 *
 * The function will only be called it was defined by the framework's user.
 */
__attribute__((weak)) void handle_disconnect_as_client();

#endif