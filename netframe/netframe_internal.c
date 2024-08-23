#include "netframe_internal.h"

static void print_packet(nf_packet_t packet) {
    printf(
        ""      "printing packet: {\n"
        "\t"        "packet_len: %d\n"
        "\t"        "packet_type: %hhu\n"
        "\t"        "client_id: %hhd\n"
        "\t"        "packet_body: ["
        ,
        packet.packet_len,
        packet.packet_type,
        packet.client_id
    );
    for (int32_t i = 0; i < packet.packet_len-2; i++) {
        if (i != 0) printf(", ");
        if (i % 20 == 0) printf("\n\t\t");
        printf("%hhx", packet.packet_body[i]);
    }
    printf(
        ""      "\n"
        "\t"        "]\n"
        ""      "}\n"
    );
}
static void print_internal_packet(nf_internal_packet_t packet) {
    printf(
        ""      "printing packet: {\n"
        "\t"        "packet_len: %d\n"
        "\t"        "packets_remaining: %hhd\n"
        "\t"        "packet_body: ["
        ,
        packet.packet_len,
        packet.packets_remaining
    );
    for (int32_t i = 0; i < packet.packet_len-1; i++) {
        if (i != 0) printf(", ");
        if (i % 20 == 0) printf("\n\t\t");
        printf("%hhx", packet.packet_body[i]);
    }
    printf(
        ""      "\n"
        "\t"        "]\n"
        ""      "}\n"
    );
}

nf_packet_t receive_packet_from_socket(socket_t* sock) {
    nf_internal_packet_t current_internal_packet;
    nf_packet_t packet_res;
    int32_t recv_return;

    packet_res.packet_len = 0;

    do {
        #ifdef NETFRAME_DEBUG
        printf("waiting for internal packet receive\n");
        #endif

        recv_return = receive_from_socket(sock, current_internal_packet.packet, _INTERNAL_PACKET_MAX_LENGTH_);
        // receive error
        if (recv_return == 0 || recv_return == SOCKET_ERROR) {
            packet_res.packet_len = 3;
            packet_res.packet_type = RECV_ERROR;
            packet_res.client_id = -1;
            packet_res.packet_body[0] = recv_return;
            return packet_res;
        }
        // recv_return is the length of the packet
        current_internal_packet.packet_len = recv_return;

        // overflow error
        if (packet_res.packet_len+current_internal_packet.packet_len-1 > _PACKET_MAX_LENGTH_) {
            packet_res.packet_len = 3;
            packet_res.packet_type = RECV_ERROR;
            packet_res.client_id = -1;
            packet_res.packet_body[0] = 1;
            return packet_res;
        }
        
        memcpy(&(packet_res.packet[packet_res.packet_len]), current_internal_packet.packet_body, current_internal_packet.packet_len-1);
        packet_res.packet_len += current_internal_packet.packet_len-1;

        #ifdef NETFRAME_DEBUG
        printf("received internal packet:\n");
        print_internal_packet(current_internal_packet);
        #endif
        
    } while(current_internal_packet.packets_remaining > 0);

    #ifdef NETFRAME_DEBUG
    printf("received complete packet:\n");
    print_packet(packet_res);
    #endif

    return packet_res;
}

int32_t send_packet_to_socket(socket_t* sock, nf_packet_t packet) {
    nf_internal_packet_t internal_packet;
    int32_t internal_packets_amount;
    int32_t packet_index = 0;
    int32_t send_result;

    #ifdef NETFRAME_DEBUG
    printf("sending complete packet:\n");
    print_packet(packet);
    #endif

    internal_packets_amount = packet.packet_len/(_INTERNAL_PACKET_MAX_LENGTH_-1);
    if (packet.packet_len % (_INTERNAL_PACKET_MAX_LENGTH_-1) > 0) internal_packets_amount += 1;
    
    for (int32_t i = 0; i < internal_packets_amount; i++) {

        internal_packet.packet_len = 1 + min(packet.packet_len - packet_index, _INTERNAL_PACKET_MAX_LENGTH_-1);
        memcpy(internal_packet.packet_body, &(packet.packet[packet_index]), internal_packet.packet_len-1);
        internal_packet.packets_remaining = internal_packets_amount - i - 1;

        #ifdef NETFRAME_DEBUG
        printf("sending internal packet:\n");
        print_internal_packet(internal_packet);
        #endif
        
        send_result = send_to_socket(sock, internal_packet.packet, internal_packet.packet_len);

        // error
        if (send_result == SOCKET_ERROR) {
            return send_result;
        }

        packet_index += internal_packet.packet_len-1;
    }
}