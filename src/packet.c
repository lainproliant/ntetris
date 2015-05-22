#include "packet.h"
#include <stdlib.h>
#include <stdio.h>

void createErrPacket(TLV *ret, ERR_CODE e)
{
    ret->type = (uint8_t)ERR_PACKET;
    ret->length = sizeof(msg_err);
    fprintf(stderr, "Creating err_packet\n");
    *(ret->value) = e;
}

void onsend(uv_udp_send_t *req, int status)
{
    fprintf(stderr, "in reply callback\n");
    if (status != 0) {
        fprintf(stderr, "Error: %s\n", uv_err_name(status));
    } else {
        fprintf(stderr, "reply callback says status was successful\n");
        free(req);
    }
}

void reply(TLV *packet, uv_udp_t *req, const struct sockaddr *from)
{
    fprintf(stderr, "in reply function\n");
    uv_udp_send_t *send_req = malloc(sizeof(uv_udp_send_t));
    uv_buf_t reply = uv_buf_init((char*)packet, sizeof(TLV));
    uv_udp_send(send_req, req, &reply, 1, from, onsend);
}
