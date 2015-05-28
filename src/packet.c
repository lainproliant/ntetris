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

bool validateLength(packet_t *p, ssize_t len, MSG_TYPE t)
{
    /* C dictates this be done outside of switch control block */
    uint8_t numLinesChanged;
    msg_update_client_state *upClient = NULL;
    msg_create_room *croom = NULL;
    msg_register_client *rclient = NULL;
    size_t totalBytes = sizeof(packet_t);

    switch(t) {
        case REGISTER_TETRAD:
            return (len == (sizeof(packet_t) + sizeof(msg_register_tetrad)));
            break;
        case UPDATE_TETRAD:
            return (len == (sizeof(packet_t) + sizeof(msg_update_tetrad)));
            break;
        case USER_ACTION:
            return (len == (sizeof(packet_t) + sizeof(msg_user_action)));
            break;

        /* Handling of variable length fields */
        case UPDATE_CLIENT_STATE:
            if (len < sizeof(msg_update_client_state)) {
                return false;
            }

            upClient = (msg_update_client_state*)p->data;
            totalBytes += sizeof(msg_update_client_state) + \
                upClient->nLinesChanged * sizeof(uint16_t); 
            return (len == totalBytes);
            break;
        case CREATE_ROOM:
            if (len < sizeof(msg_create_room)) {
                return false;
            }

            croom = (msg_create_room*)p->data;
            totalBytes += sizeof(msg_create_room) + \
                croom->roomNameLen * sizeof(unsigned char); 
            return (len == totalBytes);
            break;
        case REGISTER_CLIENT:
            if (len < sizeof(msg_register_client)) {
                return false;
            }

            rclient = (msg_register_client*)p->data;
            totalBytes += sizeof(msg_register_client) + \
                rclient->nameLength * sizeof(unsigned char);
            return (len == totalBytes);
            break;
        default:
            WARNING("WARNING: unhandled type passed in (%d)\n", t);
            return false;
            break;
    }
}
