#include "sockets.h"


uint32_t sockets_amount;
socket_t* sockets_list[_SOCKETS_MAX_AMOUNT_];



// static functions in this file
static void accept_sockets(socket_t* sock);
static int32_t clean_socket(socket_t* sock); // returns 1 if skipped terminating the current thread, otherwise 0



int32_t init_sockets() {
    WSADATA wsa_data;
    int32_t init_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (init_result != 0) return init_result;
    
    return 0;
}


socket_t* create_socket_ext(uint32_t ipaddr, uint16_t port) {
    if (sockets_amount >= _SOCKETS_MAX_AMOUNT_) return NULL;

    // allocate socket
    socket_t* sock = malloc(sizeof(socket_t));
    if (sock == NULL) return NULL;

    // socket's data
    sock->accepting_sockets = 0;
    sock->accept_thread.thread_id = -1;
    sock->handler = NULL;
    sock->accepted_socket = 0;
    sock->handler_thread.thread_id = -1;
    sock->connected_socket = 0;

    // create socket
    sock->sock = socket(
        AF_INET,
        SOCK_STREAM, // TCP
        0            // default protocol
    );
    if (sock->sock == INVALID_SOCKET) {
        free(sock);
        return NULL;
    }

    // address
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ipaddr;
    addr.sin_port = htons(port);
    sock->addr = *((struct sockaddr*)&addr);
    if (bind(sock->sock, &(sock->addr), sizeof(sock->addr)) != 0) {
        clean_socket(sock);
        return NULL;
    }

    // append socket to sockets_list
    *((uint32_t*)(&sock->socket_index)) = sockets_amount;
    sockets_list[sockets_amount] = sock;
    sockets_amount += 1;

    return sock;
}

socket_t* create_socket_local(uint16_t port) {
    return create_socket_ext(INADDR_ANY, port);
}

socket_t* create_socket(const char* ipv4addr, uint16_t port) {
    uint32_t ipaddr = inet_addr(ipv4addr);
    if (ipaddr == INADDR_NONE) return NULL;
    return create_socket_ext(ipaddr, port);
}

socket_t* create_connected_socket(const char* ipv4addr, uint16_t port) {
    if (sockets_amount >= _SOCKETS_MAX_AMOUNT_) return NULL;

    // convert ipv4addr string to an ip address
    uint32_t ipaddr = inet_addr(ipv4addr);
    if (ipaddr == INADDR_NONE) return NULL;

    // allocate socket
    socket_t* sock = malloc(sizeof(socket_t));
    if (sock == NULL) return NULL;

    // socket's data
    sock->accepting_sockets = 0;
    sock->accept_thread.thread_id = -1;
    sock->handler = NULL;
    sock->accepted_socket = 0;
    sock->handler_thread.thread_id = -1;
    sock->connected_socket = 1;

    // create socket
    sock->sock = socket(
        AF_INET,
        SOCK_STREAM, // TCP
        0            // default protocol
    );
    if (sock->sock == INVALID_SOCKET) {
        free(sock);
        return NULL;
    }

    // address to connect to
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ipaddr;
    addr.sin_port = htons(port);
    sock->addr = *((struct sockaddr*)&addr);

    // connect
    if (connect(sock->sock, &(sock->addr), sizeof(sock->addr)) != 0) {
        clean_socket(sock);
        return NULL;
    }

    // append socket to sockets_list
    *((uint32_t*)(&sock->socket_index)) = sockets_amount;
    sockets_list[sockets_amount] = sock;
    sockets_amount += 1;

    return sock;
}


static void accept_sockets(socket_t* sock) {
    sock->accepting_sockets = 1;

    SOCKET client_sock;
    struct sockaddr client_addr;
    socket_t* client;

    while (1) {
        if (sockets_amount >= _SOCKETS_MAX_AMOUNT_) continue;

        client_sock = accept(sock->sock, &client_addr, 0);
        if (client_sock == INVALID_SOCKET) continue;

        client = malloc(sizeof(socket_t));
        if (client == NULL) {
            closesocket(client_sock);
            continue;
        }

        client->sock = client_sock;
        client->addr = client_addr;
        client->accepting_sockets = 0;
        client->accept_thread.thread_id = -1;
        client->handler = NULL;
        client->accepted_socket = 1;
        client->handler_thread.thread_id = -1;
        sock->connected_socket = 1;


        // append client to sockets_list
        *((uint32_t*)(&client->socket_index)) = sockets_amount;
        sockets_list[sockets_amount] = client;
        sockets_amount += 1;

        // call handler with the newly connected socket
        if (create_thread(&(client->handler_thread), NULL, (void*(*)(void*))sock->handler, client) != 0) {
            destroy_socket(client);
            continue;
        }
    }
}

int32_t listen_and_accept_sockets_ext(socket_t* sock, int32_t backlog, void (*handler)(socket_t*)) {
    if (listen(sock->sock, backlog) != 0) return 1;

    sock->handler = handler;
    if (create_thread(&(sock->accept_thread), NULL, (void*(*)(void*))&accept_sockets, sock) != 0) return 1;

    return 0;
}

int32_t listen_and_accept_sockets(socket_t* sock, void (*handler)(socket_t*)) {
    return listen_and_accept_sockets_ext(sock, _SOCKETS_ACCEPT_DEFAULT_BACKLOG_, handler);
}


int32_t send_to_socket(socket_t* sock, char* buffer, int32_t buffer_length) {
    return send(sock->sock, buffer, buffer_length, 0);
}

int32_t receive_from_socket(socket_t* sock, char* buffer, int32_t buffer_length) {
    return recv(sock->sock, buffer, buffer_length, 0);
}


int32_t get_socket_ip(socket_t* sock) {
    struct sockaddr name;
    int32_t namelen = sizeof(struct sockaddr);
    getsockname(sock->sock, &name, &namelen);
    return ntohl(((struct sockaddr_in*)&name)->sin_addr.S_un.S_addr);
}

int32_t get_socket_connect_time(socket_t* sock) {
    int32_t connect_time;
    int32_t optlen;
    if (getsockopt(sock->sock, SOL_SOCKET, SO_CONNECT_TIME, (char*)&connect_time, &optlen) == SOCKET_ERROR) return SOCKET_ERROR;
    return connect_time;
}

int32_t get_socket_recieve_time_out(socket_t* sock) {
    int32_t timeout;
    int32_t optlen;
    if (getsockopt(sock->sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, &optlen) == SOCKET_ERROR) return SOCKET_ERROR;
    return timeout;
}

int32_t set_socket_recieve_time_out(socket_t* sock, int32_t timeout) {
    return setsockopt(sock->sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
}

int32_t ipstring_from_ipuint(char* str_buf, uint32_t ipaddr) {
    uint8_t c = 0;
    for (int8_t b = 3; b >= 0; b--) {
        uint8_t ipaddrbyte = ((uint8_t*)&ipaddr)[b];
        if (ipaddrbyte >= 100) {
            str_buf[c] = '0' + (ipaddrbyte / 100);
            c += 1;
        }
        if (ipaddrbyte >= 10) {
            str_buf[c] = '0' + ((ipaddrbyte % 100) / 10);
            c += 1;
        }
        str_buf[c] = '0' + (ipaddrbyte % 10);
        c += 1;

        if (b != 0) {
            str_buf[c] = '.';
            c += 1;
        }
    }
    str_buf[c] = '\0';
    return c;
}


static int32_t clean_socket(socket_t* sock) {
    int32_t current_thread = thread_self();
    int8_t skipped_current_thread = 0;

    // shutdown socket ; or wait until it finishes a blocking call and then shutdown
    while (shutdown(sock->sock, SD_BOTH) == WSAEINPROGRESS);

    if (sock->accepting_sockets) {
        if (current_thread == sock->accept_thread.thread_id) {
            skipped_current_thread = 1;
        }else {
            terminate_thread(sock->accept_thread);
        }
    }
    if (sock->accepted_socket) {
        if (current_thread == sock->handler_thread.thread_id) {
            skipped_current_thread = 1;
        }else {
            terminate_thread(sock->handler_thread);
        }
    }

    // close socket ; or wait until it finishes a blocking call and then close
    while (closesocket(sock->sock) == WSAEINPROGRESS);
    free(sock);

    return skipped_current_thread;
}

int32_t destroy_socket(socket_t* sock) {
    sockets_amount -= 1;

    // move the last socket at sockets_list to sockets's index
    socket_t* last_socket = sockets_list[sockets_amount];
    *((uint32_t*)&last_socket->socket_index) = sock->socket_index;
    sockets_list[last_socket->socket_index] = last_socket;

    return clean_socket(sock);
}

int32_t clean_sockets() {
    int32_t skipped_current_thread = 0;

    #ifdef DEBUG_SOFT_MODE
    printf("cleaning %u sockets\n", sockets_amount);
    #endif
    for (uint32_t i = 0; i < sockets_amount; i++) {
        if (clean_socket(sockets_list[i]) == 1) skipped_current_thread = 1;
    }
    sockets_amount = 0;

    WSACleanup();

    return skipped_current_thread;
}


void print_socket(socket_t* sock, const char* indentation) {
    printf(
        "%s{\n"
            "%s\tconst uint32_t socket_index: %u\n"
        "%s\n"
            "%s\tSOCKET sock: %u\n"
            "%s\tstruct sockaddr addr: {\n"
                "%s\t\tshort sin_family: %d\n"
                "%s\t\tu_long sin_addr.S_un.S_addr: %u\n"
                "%s\t\tu_short sin_port: %u\n"
            "%s\t}\n"
        "%s\n"
            "%s\t// for listening and accepting sockets\n"
            "%s\tuint8_t accepting_sockets: %u\n"
            "%s\tthread_t accept_thread: {\n"
                "%s\t\tint32_t thread_id: %d\n"
                "%s\t\tHANDLE thread_handle: %p\n"
            "%s\t}\n"
            "%s\tvoid (*handler)(socket_t*): %p\n"
        "%s\n"
            "%s\t// for accepted sockets\n"
            "%s\tuint8_t accepted_socket: %u\n"
            "%s\tthread_t handler_thread: {\n"
                "%s\t\tint32_t thread_id: %d\n"
                "%s\t\tHANDLE thread_handle: %p\n"
            "%s\t}\n"
        "%s}\n",
        indentation,
        indentation,    sock->socket_index,
        indentation,
        indentation,    sock->sock,
        indentation,
        indentation,    (int32_t)(((struct sockaddr_in*)&(sock->addr))->sin_family),
        indentation,    ((struct sockaddr_in*)&(sock->addr))->sin_addr.S_un.S_addr,
        indentation,    (uint32_t)(((struct sockaddr_in*)&(sock->addr))->sin_port),
        indentation,
        indentation,
        indentation,
        indentation,    (uint32_t)sock->accepting_sockets,
        indentation,
        indentation,    sock->accept_thread.thread_id,
        indentation,    sock->accept_thread.thread_handle,
        indentation,
        indentation,    sock->handler,
        indentation,
        indentation,
        indentation,    (uint32_t)sock->accepted_socket,
        indentation,
        indentation,    sock->handler_thread.thread_id,
        indentation,    sock->handler_thread.thread_handle,
        indentation,
        indentation
    );
}