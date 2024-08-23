#include "netframe.h"
#include "netframe_internal.h"

// packets that will go out to the server
static nf_packet_t packets_stack[_PACKETS_STACK_LENGTH_];
static int32_t packets_stack_size;

static socket_t* server_outsock;
static socket_t* server_insock;

static thread_t out_handler_thread;
static thread_t in_handler_thread;

static int8_t client_id;
static uint8_t connected;
static uint8_t connection_failed;


static void out_handler();
static void in_handler();


int32_t join_server(const char* ipv4addr) {
    // initiallise sockets API
    if (init_sockets() != 0) {
        // failed to initiallised sockets API
        return -1;
    }
    
    // create server sockets connections
    server_outsock = create_connected_socket(ipv4addr, _SERVER_OUT_PORT_);
    server_insock = create_connected_socket(ipv4addr, _SERVER_IN_PORT_);
    if (server_outsock == NULL || server_insock == NULL) {
        // failed to create 1 or more server sockets connections
        clean_sockets();
        return -1;
    }

    packets_stack_size = 0;
    client_id = -1;
    connected = 0;
    connection_failed = 0;

    // start in/out handlers
    if (create_thread(&out_handler_thread, NULL, (void*(*)(void*))&out_handler, NULL) != 0) {
        // failed to run out_handler on a new thread
        clean_sockets();
        return -1;
    }
    if (create_thread(&in_handler_thread,  NULL, (void*(*)(void*))&in_handler,  NULL) != 0) {
        // failed to run in_handler on a new thread
        terminate_thread(out_handler_thread);
        clean_sockets();
        return -1;
    }

    // wait until connected or connection failed
    while (connected == 0 && connection_failed == 0);
    if (connection_failed) {
        // connection failed
        return -1;
    }

    // connected successfully
    return client_id;
}

void exit_server() {
    int32_t current_thread = thread_self();
    uint8_t use_exit_thread = 0;

    if (current_thread == out_handler_thread.thread_id) {
        use_exit_thread = 1;
    }else {
        terminate_thread(out_handler_thread);
    }
    
    if (current_thread == in_handler_thread.thread_id) {
        use_exit_thread = 1;
    }else {
        terminate_thread(in_handler_thread);
    }

    if (clean_sockets() == 1) {
        use_exit_thread = 1;
    }

    if (handle_disconnect_as_client != NULL) handle_disconnect_as_client();

    if (use_exit_thread == 1) exit_thread(0);
}

void send_update_packet(nf_packet_t packet) {
    if (packet.packet_type != CLIENT_UPDATE) return;

    // append packet
    packets_stack[packets_stack_size] = packet;
    packets_stack_size += 1;
}



static void out_handler() {
    int32_t send_result;

    #ifdef NETFRAME_DEBUG
    printf("out_handler %u: server-in-socket accepted connection.\n", server_insock->sock);
    #endif

    // wait until client ID is assigned
    while (client_id == -1);
    
    // send CLIENT_OUT_SOCKET_CONNECT packet
    if(1){      // create a scope to remove `packet` from stack after use
    #ifdef NETFRAME_DEBUG
    printf("out_handler %u: sending CLIENT_OUT_SOCKET_CONNECT with client id %d to server-in-socket.\n", server_insock->sock, client_id);
    #endif

    nf_packet_t packet = (nf_packet_t) {
        .packet_len = 2,
        .packet_type = CLIENT_OUT_SOCKET_CONNECT,
        .client_id = (int8_t)client_id
    };
    packets_stack[packets_stack_size] = packet;
    packets_stack_size += 1;
    }           // end of the scope, remove `packet` from stack after use


    // send packets loop
    while(1) {
        if (packets_stack_size <= 0) continue;

        #ifdef NETFRAME_DEBUG
        printf("out_handler %u: sending packet to server-in-socket.\n", server_insock->sock);
        #endif

        // send packet from top of the stack and pop it
        packets_stack_size -= 1;
        
        send_result = send_packet_to_socket(server_insock, packets_stack[packets_stack_size]);

        // error
        if (send_result == SOCKET_ERROR) {
            #ifdef NETFRAME_DEBUG
            printf("out_handler %u: ERROR: buffer to server-in-socket did not arrive.\n", server_insock->sock);
            printf("out_handler %u: disconnecting.\n", server_insock->sock);
            #endif
            exit_server();
        }

        // CLIENT_OUT_SOCKET_CONNECT packet arrived successfully - connected
        if (connected == 0) {
            #ifdef NETFRAME_DEBUG
            printf("out_handler %u: first buffer to server-in-socket arrived - connected.\n", server_insock->sock);
            #endif
            connected = 1;
        }
    }
}

static void in_handler() {
    nf_packet_t server_packet;

    #ifdef NETFRAME_DEBUG
    printf("in_handler %u: server-out-socket %u accepted connection.\n", server_outsock->sock, server_outsock->sock);
    #endif

    while(1) {
        server_packet = receive_packet_from_socket(server_outsock);

        switch (server_packet.packet_type) {
            case RECV_ERROR: {
                #ifdef NETFRAME_DEBUG
                if (server_packet.packet_body[0] == 0)            printf("in_handler %u: ERROR: connection with server-out-socket ended.\n", server_outsock->sock);
                if (server_packet.packet_body[0] == SOCKET_ERROR) printf("in_handler %u: ERROR: buffer from server-out-socket did not arrive.\n", server_outsock->sock);
                
                printf("in_handler %u: disconnecting.\n", server_outsock->sock);
                #endif
                exit_server();
                break;
            }
            
            case SERVER_ASSIGNED_CLIENT_ID: {
                client_id = server_packet.client_id;

                #ifdef NETFRAME_DEBUG
                printf("in_handler %u: received SERVER_ASSIGNED_CLIENT_ID packet with client ID %d.\n", server_outsock->sock, (int32_t)client_id);
                #endif

                // client id errors
                if (client_id < 0 || client_id >= _CLIENTS_MAX_AMOUNT_) {
                    #ifdef NETFRAME_DEBUG
                    printf("in_handler %u: ERROR: non existing client ID.\n", server_outsock->sock);
                    printf("in_handler %u: disconnecting.\n", server_outsock->sock);
                    #endif
                    exit_server();
                }

                #ifdef NETFRAME_DEBUG
                printf("in_handler %u: received client ID successfully.\n", server_outsock->sock);
                #endif
                break;
            }

            case SERVER_NO_CLIENT_IDS: {
                #ifdef NETFRAME_DEBUG
                printf("in_handler %u: ERROR: received SERVER_NO_CLIENT_IDS packet.\n", server_outsock->sock);
                #endif
                exit_server();
                break;
            }

            case SERVER_STATE: {
                #ifdef NETFRAME_DEBUG
                printf("in_handler %u: received SERVER_STATE packet.\n", server_outsock->sock);
                #endif
                if (parse_state_packet != NULL) parse_state_packet(server_packet);
                break;
            }

            case SERVER_CLIENT_CONNECTED: {
                #ifdef NETFRAME_DEBUG
                printf("in_handler %u: received SERVER_CLIENT_CONNECTED packet.\n", server_outsock->sock);
                #endif
                if (handle_client_connect != NULL) handle_client_connect(server_packet.client_id);
                break;
            }

            case SERVER_CLIENT_DISCONNECTED: {
                #ifdef NETFRAME_DEBUG
                printf("in_handler %u: received SERVER_CLIENT_DISCONNECTED packet.\n", server_outsock->sock);
                #endif
                if (handle_client_disconnect != NULL) handle_client_disconnect(server_packet.client_id);
                break;
            }

            case SERVER_CLIENT_UPDATE: {
                #ifdef NETFRAME_DEBUG
                printf("in_handler %u: received SERVER_CLIENT_UPDATE packet.\n", server_outsock->sock);
                #endif
                if (parse_update_packet != NULL) parse_update_packet(server_packet);
                break;
            }

            // error
            default: {
                #ifdef NETFRAME_DEBUG
                printf("in_handler %u: ERROR: received packet with bad type.\n", server_outsock->sock);
                #endif
                break;
            }
        }
    }
}



