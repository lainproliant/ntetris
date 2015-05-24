#include "packet.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "macros.h"
#include <sys/socket.h>

void createErrPacket(packet_t *ret, ERR_CODE e)
{
    ret->type = (uint8_t)ERR_PACKET;
    *(ret->data) = e;
}

void onsend(uv_udp_send_t *req, int status)
{
    fprintf(stderr, "in reply callback\n");
    if (status != 0) {
        WARNING("WARNING: %s\n", uv_err_name(status));
    } else {
        fprintf(stderr, "reply callback says status was successful\n");
        free(req);
    }
}

void reply(packet_t *p, size_t psize, uv_udp_t *req, const struct sockaddr *from, int sock)
{
    /*fprintf(stderr, "in reply function\n");
    uv_udp_send_t *send_req = malloc(sizeof(uv_udp_send_t));
    uv_buf_t reply = uv_buf_init((char*)p, psize);
    uv_udp_send(send_req, req, &reply, 1, from, onsend);*/
    if(sendto(sock, p, psize, NULL, from, sizeof(struct sockaddr)) < 0) {
        WARNING("WARNING: something went wrong in the reply - %s\n", strerror(errno));
    }
}

size_t pack_msg_err(msg_err *m, packet_t **buf)
{
    if (*buf == NULL) {
        *buf = malloc(ERRMSG_SIZE);
    }

    (*buf)->type = ERR_PACKET;
    (*buf)->data[0] = m->badMsgCode;
    return ERRMSG_SIZE;
}

size_t pack_msg_register_tetrad(msg_register_tetrad *m, packet_t **buf)
{
    printf("packing register tetrad\n");
    return 0;
}

size_t pack_msg_register_client(msg_register_client *m, packet_t **buf)
{
    return 0;
}

size_t pack_msg_update_client(msg_update_client_state *m, packet_t **buf)
{
    return 0;
}

size_t pack_msg_update_tetrad(msg_update_tetrad *m, packet_t **buf)
{
    return 0;
}
size_t pack_msg_create_room(msg_create_room *m, packet_t **buf)
{
    return 0;
}

size_t pack_user_action(msg_user_action *m, packet_t **buf)
{
    return 0;
}
