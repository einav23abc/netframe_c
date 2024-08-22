#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "netframe/netframe.h"


#define _INPUT_BUFFER_LENGTH_ (256)

#define _SERVER_USER_ID_ (_CLIENTS_MAX_AMOUNT_)



uint8_t is_server;
uint8_t is_client;
uint32_t client_id;

typedef struct {
    uint8_t connected;

    int8_t x;
    int8_t y;
} user_t;

user_t users[_CLIENTS_MAX_AMOUNT_+1];
uint32_t users_amount;

enum UPDATE_TYPE {
    SETX,
    SETY
};


void print_users_data();


int32_t main() {
    char input_buffer[_INPUT_BUFFER_LENGTH_] = {0};
    int32_t func_res;


    printf("Welcome to netframe showcase.\n");
    printf("Type 'help' for help.\n");

    is_server = 0;
    is_client = 0;

    while(1) {
        // get input command
        fgets(input_buffer, _INPUT_BUFFER_LENGTH_-1, stdin);
        fflush(stdin);
        // remove newline
        input_buffer[strlen(input_buffer)-1] = '\0';


        if (strcmp(input_buffer, "help") == 0) {
            printf(
                "Commands:\n"
                "\thelp             - print this message.\n"
                "\texit             - exit the program.\n"
                "\topen server      - open a server localy.\n"
                "\tclose server     - close the server.\n"
                "\tjoin server ____ - join a server at ip ____ as a client. the ip should be typed in the xxx.xxx.xxx.xxx format.\n"
                "\texit server      - exit the server as a client.\n"
                "\tprint            - print the shared object.\n"
                "\tsetx ____        - set the x value of your client to ____ at the shared object.\n"
                "\tsety ____        - set the y value of your client to ____ at the shared object.\n"
            );
            continue;
        }

        if (strcmp(input_buffer, "exit") == 0) {
            clean_sockets();
            return 0;
        }

        if (strcmp(input_buffer, "open server") == 0) {
            if (is_client == 1) {
                printf("can not open a server while connected to a server\n");
                continue;
            }
            if (is_server == 1) {
                printf("can not open a server while hosting a server\n");
                continue;
            }

            func_res = open_server_local();
            if (func_res != 0) {
                printf("failed to open server\n");
                continue;
            }

            printf("opened a server\n");
            is_server = 1;
            client_id = -1;

            // init server data
            for (uint32_t i = 0; i < _CLIENTS_MAX_AMOUNT_+1; i++) users[i].connected = 0;
            users[_SERVER_USER_ID_] = (user_t){
                .connected = 1,
                .x = 0, .y = 0
            };
            users_amount = 1;
            continue;
        }
        
        if (strcmp(input_buffer, "close server") == 0) {
            if (is_server == 0) {
                printf("can not close a non existent server\n");
                continue;
            }

            printf("closing server\n");
            close_server();
            is_server = 0;
            continue;
        }
        
        if (memcmp(input_buffer, "join server ", 12) == 0) {
            if (is_server == 1) {
                printf("can not connect to a server while hosting a server\n");
                continue;
            }
            if (is_client == 1) {
                printf("can not connect to a server while connected to server\n");
                continue;
            }
            if (strlen(input_buffer) <= 12) {
                printf("you must enter the ip of the server in the format xxx.xxx.xxx.xxx\n");
                continue;
            }

            func_res = join_server(&(input_buffer[12]));
            if (func_res < 0) {
                printf("failed to join server\n");
                continue;
            }

            printf("joined server. client id is %d\n", func_res);
            is_client = 1;
            client_id = func_res;
            continue;
        }
        
        if (strcmp(input_buffer, "exit server") == 0) {
            if (is_client == 0) {
                printf("can not exit a server while not connected to server\n");
                continue;
            }

            printf("exiting server\n");
            exit_server();
            is_client = 0;
            continue;
        }
        
        if (strcmp(input_buffer, "print") == 0) {
            print_users_data();
            continue;
        }
        
        if (memcmp(input_buffer, "setx ", 5) == 0) {
            if (is_client == 0 && is_server == 0) {
                printf("cannot set X while not connected to a server or hosting one.\n");
                continue;
            }

            int8_t new_x = atoi(&(input_buffer[4]));
            
            if (is_server) {
                users[_CLIENTS_MAX_AMOUNT_].x = new_x;
            }else {
                users[client_id].x = new_x;
            }

            // create CLIENT_UPDATE packet
            client_packet_t packet = (client_packet_t) {
                .packet_len = 3,
                .packet_type = CLIENT_UPDATE,
                .packet_body[0] = SETX,
                .packet_body[1] = new_x
            };

            if (is_server) {
                send_update_packet_as_server(packet);
            }else {
                send_update_packet(packet);
            }
            continue;
        }
        
        if (memcmp(input_buffer, "sety ", 5) == 0) {
            if (is_client == 0 && is_server == 0) {
                printf("cannot set Y while not connected to a server or hosting one.\n");
                continue;
            }

            int8_t new_y = atoi(&(input_buffer[4]));
            
            if (is_server) {
                users[_CLIENTS_MAX_AMOUNT_].y = new_y;
            }else {
                users[client_id].y = new_y;
            }

            // create CLIENT_UPDATE packet
            client_packet_t packet = (client_packet_t) {
                .packet_len = 3,
                .packet_type = CLIENT_UPDATE,
                .packet_body[0] = SETY,
                .packet_body[1] = new_y
            };

            if (is_server) {
                send_update_packet_as_server(packet);
            }else {
                send_update_packet(packet);
            }
            continue;
        }
    }
}



void print_users_data() {
    printf("users: [\n");
    for (uint32_t i = 0; i < _CLIENTS_MAX_AMOUNT_+1; i++) {
        printf("\t{con: %d,\tx: %d,\ty: %d}\n", users[i].connected, users[i].x, users[i].y);
    }
    printf("]\n");
}



server_packet_t generate_state_packet() {
    server_packet_t packet = (server_packet_t){
        .packet_len = 2,
        .packet_type = SERVER_STATE,
        .client_id = -1
    };
    for (uint32_t i = 0; i < _CLIENTS_MAX_AMOUNT_+1; i++) {
        packet.packet_body[i*3    ] = users[i].connected;
        packet.packet_body[i*3 + 1] = users[i].x;
        packet.packet_body[i*3 + 2] = users[i].y;
    }
    packet.packet_len += (_CLIENTS_MAX_AMOUNT_+1)*3;

    return packet;
}

void parse_state_packet(server_packet_t packet) {
    for (uint32_t i = 0; i < _CLIENTS_MAX_AMOUNT_+1; i++) {
        users[i].connected = packet.packet_body[i*3    ];
        users[i].x         = packet.packet_body[i*3 + 1];
        users[i].y         = packet.packet_body[i*3 + 2];
    }
    
    print_users_data();
}

void parse_update_packet(server_packet_t packet) {
    if (packet.client_id == client_id) return;

    int32_t user_id = packet.client_id;
    if (packet.client_id == -1) user_id = _CLIENTS_MAX_AMOUNT_;

    switch (packet.packet_body[0]) {
        case SETX: {
            users[user_id].x = packet.packet_body[1];
            break;
        }
        case SETY: {
            users[user_id].y = packet.packet_body[1];
            break;
        }
    }
}

void handle_client_connect(int32_t client_id) {
    printf("client %d connected\n", client_id);

    users[client_id].connected = 1;
    users[client_id].x = 0;
    users[client_id].y = 0;
}

void handle_client_disconnect(int32_t client_id) {
    printf("client %d disconnected\n", client_id);
    
    users[client_id].connected = 0;
}

void handle_disconnect_as_client() {
    printf("disconnected as client\n");
    is_client = 0;
}

