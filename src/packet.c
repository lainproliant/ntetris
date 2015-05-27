#include "packet.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "macros.h"
#include <alloca.h>
#include <sys/socket.h>

void createErrPacket(packet_t *ret, ERR_CODE e)
{
    ret->type = (uint8_t)ERR_PACKET;
    *(ret->data) = e;
}

void reply(packet_t *p, size_t psize, const struct sockaddr *from, int sock)
{
    if(sendto(sock, p, psize, NULL, from, sizeof(struct sockaddr)) < 0) {
        WARNING("WARNING: something went wrong in the reply - %s\n", strerror(errno));
    }
}

