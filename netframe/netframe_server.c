#include "netframe.h"
#include "netframe_internal.h"

typedef struct {
    uint8_t active                   : 1;
    uint8_t client_outsock_connected : 1;
    socket_t* client_outsock;
    uint8_t client_insock_connected  : 1;
    socket_t* client_insock;

    // packets that will go out to the client
    nf_packet_t packets_stack[_PACKETS_STACK_LENGTH_];
    int32_t packets_stack_size;
} client_t;


static client_t clients[_CLIENTS_MAX_AMOUNT_];
static int8_t clients_amount;

static socket_t* server_outsock;
static socket_t* server_insock;


static void broadcast_packet_to_client_stacks(nf_packet_t* packet, int8_t skip_client);
static void broadcast_client_disconnect(int8_t client_id);
static void disconnect_client(int8_t client_id);
static void out_handler(socket_t* client_insock);
static void in_handler(socket_t* client_outsock);


static int32_t open_server_ext(uint32_t ipaddr) {
    clients_amount = 0;
    for (uint32_t i = 0; i < _CLIENTS_MAX_AMOUNT_; i++) clients[i].active = 0;


    // initiallise sockets API
    if (init_sockets() != 0) {
        // failed to initiallise sockets API
        return 1;
    }
    
    // create server sockets
    server_outsock = create_socket_ext(ipaddr, _SERVER_OUT_PORT_);
    server_insock = create_socket_ext(ipaddr, _SERVER_IN_PORT_);
    if (server_outsock == NULL || server_insock == NULL) {
        // failed to create 1 or more server sockets
        clean_sockets();
        return 1;
    }

    // listen and accept connections
    if (
        listen_and_accept_sockets(server_outsock, &out_handler) != 0 ||
        listen_and_accept_sockets(server_insock, &in_handler)   != 0
    ) {
        // failed to start a listening to socket connections from server
        clean_sockets();
        return 1;
    }

    // initiallised server succefully
    return 0;
}

int32_t open_server_local() {
    return open_server_ext(INADDR_ANY);
}

int32_t open_server(const char* ipv4addr) {
    uint32_t ipaddr = inet_addr(ipv4addr);
    if (ipaddr == INADDR_NONE) return 1;
    return open_server_ext(ipaddr);
}

uint32_t get_server_ip() {
    uint32_t ip = ((struct sockaddr_in*)&server_insock->addr)->sin_addr.S_un.S_addr;
    if (ip == INADDR_ANY) {
        // get 1 of the local ip addresses
        char host_name_buff[128];
        gethostname(host_name_buff, 128);
        HOSTENT* host = gethostbyname(host_name_buff);
        ip = ((struct sockaddr_in*)host->h_addr_list[0])->sin_addr.S_un.S_addr;
    }
    return ntohl(ip);
}

void close_server() {
    clean_sockets();
}

void send_update_packet_as_server(nf_packet_t packet) {
    if (packet.packet_type != CLIENT_UPDATE) return;

    // broadcast SERVER_CLIENT_UPDATE packet
    nf_packet_t server_packet;
    memcpy(server_packet.packet, packet.packet, _PACKET_MAX_LENGTH_);
    server_packet.packet_len = packet.packet_len;
    server_packet.packet_type = SERVER_CLIENT_UPDATE;

    broadcast_packet_to_client_stacks(&server_packet, -1);
    if (parse_update_packet != NULL) parse_update_packet(server_packet);
}


static void broadcast_packet_to_client_stacks(nf_packet_t* packet, int8_t skip_client) {
    for (int8_t i = 0; i < _CLIENTS_MAX_AMOUNT_; i++) {
        if (i == skip_client) continue;
        if (clients[i].active == 0) continue;

        clients[i].packets_stack[clients[i].packets_stack_size] = *packet; 
        clients[i].packets_stack_size += 1;
    }
}

static void broadcast_client_disconnect(int8_t client_id) {
    // broadcast SERVER_CLIENT_DISCONNECTED packet
    nf_packet_t server_packet = (nf_packet_t){
        .packet_len = 2,
        .packet_type = SERVER_CLIENT_DISCONNECTED,
        .client_id = client_id
    };
    broadcast_packet_to_client_stacks(&server_packet, client_id);
    if (handle_client_disconnect != NULL) handle_client_disconnect(client_id);
}

static void disconnect_client(int8_t client_id) {
    if (client_id < 0 || client_id >= _CLIENTS_MAX_AMOUNT_) return;
    if (clients[client_id].active == 0) return;

    clients[client_id].active = 0;
    clients[client_id].packets_stack_size = 0;

    // broadcast SERVER_CLIENT_DISCONNECTED packet
    broadcast_client_disconnect(client_id);
    
    uint8_t use_exit_thread = 0;

    // destroy client-out-socket and in-handler-thread (unless in-handler-thread is the current thread)
    if (clients[client_id].client_outsock_connected) {
        clients[client_id].client_outsock_connected = 0;

        if (destroy_socket(clients[client_id].client_outsock) == 1) {
            use_exit_thread = 1;
        }
    }

    // destroy client-in-socket and out-handler-thread
    if (clients[client_id].client_insock_connected) {
        clients[client_id].client_insock_connected = 0;

        if (destroy_socket(clients[client_id].client_insock) == 1) {
            use_exit_thread = 1;
        }
    }

    if (use_exit_thread) exit_thread(0);
}


static void out_handler(socket_t* client_insock) {
    int32_t send_result;
    int8_t client_id;

    #ifdef NETFRAME_DEBUG
    printf("client XX out_handler %u: accepted client-in-socket connection.\n", client_insock->sock);
    #endif

    // assign client ID
    client_id = -1;
    for (uint32_t i = 0; i < _CLIENTS_MAX_AMOUNT_; i++) {
        if (clients[i].active == 0) {
            clients[i].active = 1;
            clients[i].client_outsock_connected = 0;
            clients[i].client_outsock = NULL;
            clients[i].client_insock_connected = 1;
            clients[i].client_insock = client_insock;
            clients[i].packets_stack_size = 0;

            client_id = i;
            break;
        }
    }

    if (client_id == -1) {
        // no avaiable client IDs, send SERVER_NO_CLIENT_IDS packet

        #ifdef NETFRAME_DEBUG
        printf("client XX out_handler %u: sending SERVER_NO_CLIENT_IDS to client-in-socket.\n", client_insock->sock);
        #endif

        nf_packet_t packet = (nf_packet_t) {
            .packet_len = 2,
            .packet_type = SERVER_NO_CLIENT_IDS,
            .client_id = -1
        };
        // send_to_socket(client_insock, packet.packet, packet.packet_len);
        send_packet_to_socket(client_insock, packet);

        #ifdef NETFRAME_DEBUG
        printf("out_handler %u: terminating.\n", client_insock->sock);
        #endif
        destroy_socket(client_insock); // wont destroy this thread
        exit_thread(0);
    }
    

    // assigned a client ID, append SERVER_ASSIGNED_CLIENT_ID packet to client's packets stack
    if(1){      // create a scope to remove `packet` from stack after use
    #ifdef NETFRAME_DEBUG
    printf("client %02d out_handler %u: sending SERVER_ASSIGNED_CLIENT_ID with client id %d to client-in-socket.\n", client_id, client_insock->sock, client_id);
    #endif

    nf_packet_t packet = (nf_packet_t) {
        .packet_len = 2,
        .packet_type = SERVER_ASSIGNED_CLIENT_ID,
        .client_id = client_id
    };
    clients[client_id].packets_stack[clients[client_id].packets_stack_size] = packet;
    clients[client_id].packets_stack_size += 1;
    }           // end of the scope, remove `packet` from stack after use

    
    // send packets loop
    while(1) {
        if (clients[client_id].packets_stack_size <= 0) continue;

        #ifdef NETFRAME_DEBUG
        printf("client %02d out_handler %u: sending packet to client-in-socket\n", client_id, client_insock->sock);
        #endif

        // send packet from top of the stack and pop it
        clients[client_id].packets_stack_size -= 1;
        
        send_result = send_packet_to_socket(client_insock, clients[client_id].packets_stack[clients[client_id].packets_stack_size]);

        // error
        if (send_result == SOCKET_ERROR) {
            #ifdef NETFRAME_DEBUG
            printf("client %02d out_handler %u: ERROR: buffer to client-in-socket did not arrive.\n", client_id, client_insock->sock);
            printf("client %02d out_handler %u: disconnecting.\n", client_id, client_insock->sock);
            #endif
            disconnect_client(client_id); // destroys this thread too
        }
    }
}

static void in_handler(socket_t* client_outsock) {
    nf_packet_t client_packet;
    nf_packet_t server_packet;
    int8_t client_id = -1;
    uint8_t connected = 0;

    #ifdef NETFRAME_DEBUG
    printf("client XX in_handler %u: accepted client-out-socket connection.\n", client_outsock->sock);
    #endif

    while(1) {
        client_packet = receive_packet_from_socket(client_outsock);

        switch (client_packet.packet_type) {
            case RECV_ERROR: {
                #ifdef NETFRAME_DEBUG
                if (client_packet.packet_body[0] == 0)            printf("client %02d in_handler %u: ERROR: connection with client-out-socket ended.\n", client_id, client_outsock->sock);
                if (client_packet.packet_body[0] == SOCKET_ERROR) printf("client %02d in_handler %u: ERROR: buffer from client-out-socket did not arrive.\n", client_id, client_outsock->sock);
                
                printf("client %02d in_handler %u: disconnecting.\n", client_id, client_outsock->sock);
                #endif
                if (connected == 0) {
                    destroy_socket(client_outsock); // wont destroy this thread
                    exit_thread(0);
                }
                disconnect_client(client_id); // destroys this thread too
                break;
            }

            case CLIENT_OUT_SOCKET_CONNECT: {
                if (connected == 1) break; // what the fuck

                client_id = (int8_t)(client_packet.client_id);

                #ifdef NETFRAME_DEBUG
                printf("client %02d in_handler %u: received CLIENT_OUT_SOCKET_CONNECT packet.\n", client_id, client_outsock->sock);                
                #endif

                // client id errors
                if (client_id < 0 || client_id >= _CLIENTS_MAX_AMOUNT_) {
                    #ifdef NETFRAME_DEBUG
                    printf("client %02d in_handler %u: ERROR: non existing client ID.\n", client_id, client_outsock->sock);
                    printf("client %02d in_handler %u: disconnecting.\n", client_id, client_outsock->sock);
                    #endif
                    destroy_socket(client_outsock); // wont destroy this thread
                    exit_thread(0);
                }
                if (clients[client_id].client_outsock_connected == 1 || clients[client_id].active == 0) {
                    #ifdef NETFRAME_DEBUG
                    printf("client %02d in_handler %u: ERROR: client ID isnt waiting for connection.\n", client_id, client_outsock->sock);
                    printf("client %02d in_handler %u: disconnecting.\n", client_id, client_outsock->sock);
                    #endif
                    destroy_socket(client_outsock); // wont destroy this thread
                    exit_thread(0);
                }

                #ifdef NETFRAME_DEBUG
                printf("client %02d in_handler %u: connected successfully.\n", client_id, client_outsock->sock);
                #endif
                clients[client_id].client_outsock_connected = 1;
                clients[client_id].client_outsock = client_outsock;
                connected = 1;

                // broadcast SERVER_CLIENT_CONNECTED packet
                server_packet = (nf_packet_t){
                    .packet_len = 2,
                    .packet_type = SERVER_CLIENT_CONNECTED,
                    .client_id = client_id
                };
                broadcast_packet_to_client_stacks(&server_packet, client_id);
                if (handle_client_connect != NULL) handle_client_connect(client_id);

                // send SERVER_STATE packet to connected client, by appending to packets stack
                if (generate_state_packet != NULL) {
                    server_packet = generate_state_packet();
                    if (server_packet.packet_type == SERVER_STATE) {
                        clients[client_id].packets_stack[clients[client_id].packets_stack_size] = server_packet;
                        clients[client_id].packets_stack_size += 1;
                    }
                }
                break;
            }
            
            case CLIENT_UPDATE: {
                #ifdef NETFRAME_DEBUG
                printf("client %02d in_handler %u: received CLIENT_UPDATE packet.\n", client_id, client_outsock->sock);
                #endif

                // broadcast SERVER_CLIENT_UPDATE packet
                server_packet = (nf_packet_t){
                    .packet_len = min(1 + client_packet.packet_len, _PACKET_MAX_LENGTH_),
                    .packet_type = SERVER_CLIENT_UPDATE,
                    .client_id = client_id
                };
                memcpy(server_packet.packet_body, client_packet.packet_body, min(client_packet.packet_len-1, _PACKET_MAX_LENGTH_-2));
                broadcast_packet_to_client_stacks(&server_packet, client_id);
                if (parse_update_packet != NULL) parse_update_packet(server_packet);
                break;
            }

            // error
            default: {
                #ifdef NETFRAME_DEBUG
                printf("client %02d in_handler %u: ERROR: received packet with bad type.\n", client_id, client_outsock->sock);
                
                printf("client %02d in_handler %u: disconnecting.\n", client_id, client_outsock->sock);
                #endif
                if (connected == 0) {
                    destroy_socket(client_outsock); // wont destroy this thread
                    exit_thread(0);
                }
                disconnect_client(client_id); // destroys this thread too
                break;
            }
        }
    }
}