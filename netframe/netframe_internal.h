#ifndef NETFRAME_INTERNAL_H
#define NETFRAME_INTERNAL_H

#include "netframe.h"

#ifndef _INTERNAL_PACKET_MAX_LENGTH_
#define _INTERNAL_PACKET_MAX_LENGTH_ (128)
#endif

#pragma pack(1)
typedef struct {
    int32_t packet_len;

    union {
        char packet[_INTERNAL_PACKET_MAX_LENGTH_];
        struct {
            int8_t packets_remaining;
            char packet_body[_INTERNAL_PACKET_MAX_LENGTH_-1];
        };
    };
} nf_internal_packet_t;
#pragma pack()


/* \brief Get a nf_packet_t from a socket.
 *
 * If an error ocoured the packet will be of type `RECV_ERROR`.
 */
nf_packet_t receive_packet_from_socket(socket_t* sock);
/* \brief Send a nf_packet_t to a socket.
 * 
 * \returns 0 on success or `SOCKET_ERROR` if a buffer to socket did not arrive.
 */
int32_t send_packet_to_socket(socket_t* sock, nf_packet_t packet);


#endif