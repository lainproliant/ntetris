#include "packet.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "macros.h"
#include <errno.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void createErrPacket(packet_t *ret, ERR_CODE e)
{
    ret->type = (uint8_t)ERR_PACKET;
    ret->version = PROTOCOL_VERSION;
    *(ret->data) = e;
}

void reply(packet_t *p, size_t psize, const struct sockaddr *from, int sock)
{
    setProtocolVers(p);
    if(sendto(sock, p, psize, 0, from, sizeof(struct sockaddr)) < 0) {
        WARNING("something went wrong in the reply - %s", strerror(errno));
    }
}

void setProtocolVers(packet_t *p)
{
    p->version = PROTOCOL_VERSION;
}

/* If this function doesn't modify expectedSize, assume malformed packet */
bool validateLength(packet_t *p, ssize_t len, MSG_TYPE t, ssize_t *expectedSize)
{
    /* C dictates this be done outside of switch control block */
    msg_update_client_state *upClient = NULL;
    msg_create_room *croom = NULL;
    msg_register_client *rclient = NULL;
    msg_kick_client *kclient = NULL;
    msg_join_room *jroom = NULL;
    msg_room_announce *aroom = NULL;
    ssize_t totalBytes = sizeof(packet_t);

    switch(t) {
        case REGISTER_TETRAD:
            totalBytes += sizeof(msg_register_tetrad);
            break;
        case UPDATE_TETRAD:
            totalBytes += sizeof(msg_update_tetrad);
            break;
        case USER_ACTION:
            totalBytes += sizeof(msg_user_action);
            break;
        case REG_ACK:
            totalBytes += sizeof(msg_reg_ack);
            break;
        case PING:
            totalBytes += sizeof(msg_ping);
            break;
        case DISCONNECT_CLIENT:
            totalBytes += sizeof(msg_disconnect_client);
            break;
        case LIST_ROOMS:
            totalBytes += sizeof(msg_list_rooms);
            break;

        /* Handling of variable length fields */
        case UPDATE_CLIENT_STATE:
            if (len < sizeof(msg_update_client_state) + totalBytes) {
                return false;
            }

            upClient = (msg_update_client_state*)p->data;
            totalBytes += sizeof(msg_update_client_state) + \
                upClient->nLinesChanged * sizeof(uint16_t); 
            break;
        case CREATE_ROOM:
            if (len < sizeof(msg_create_room) + totalBytes) {
                return false;
            }

            croom = (msg_create_room*)p->data;
            totalBytes += sizeof(msg_create_room) + \
                croom->roomNameLen * sizeof(char) + \
                croom->passLen * sizeof(char);
            break;
        case REGISTER_CLIENT:
            if (len < sizeof(msg_register_client) + totalBytes) {
                return false;
            }

            rclient = (msg_register_client*)p->data;
            totalBytes += sizeof(msg_register_client) + \
                rclient->nameLength * sizeof(char);
            break;
        case KICK_CLIENT:
            if (len < sizeof(msg_kick_client) + totalBytes) {
                return false;
            }

            kclient = (msg_kick_client*)p->data;
            totalBytes += sizeof(msg_kick_client) + \
                ntohs(kclient->reasonLength) * sizeof(char);
            break;
        case JOIN_ROOM:
            if (len < sizeof(msg_join_room) + totalBytes) {
                return false;
            }

            jroom = (msg_join_room*)p->data;
            totalBytes += sizeof(msg_join_room) + \
                ntohs(jroom->passwordLen) * sizeof(char);
            break;
        case ROOM_ANNOUNCE:
            if (len < sizeof(msg_room_announce) + totalBytes) {
                return false;
            }

            aroom = (msg_room_announce*)p->data;
            totalBytes += sizeof(msg_room_announce) + \
                ntohs(aroom->roomNameLen) * sizeof(char);
            break;
        default:
            WARNING("unhandled type passed in (%d)", t);
            return false;
            break;
    }
            *expectedSize = totalBytes;
            return (len == totalBytes);
}

bool validateName(msg_register_client *m)
{
    size_t i;

    if (m->nameLength > MAX_NAMELEN || m->nameLength == 0) {
        return false;
    } else {
        for (i = 0; i < m->nameLength; ++i) {
            if (!isprint(m->name[i])) {
                return false;
            }
        }
    }

    return true;
}

bool validateRoomName(msg_create_room *m)
{
    if (m->roomNameLen > MAX_NAMELEN || m->roomNameLen == 0) {
        return false;
    } else {
        for (size_t i = 0; i < m->roomNameLen; ++i) {
            if (!isprint(m->roomNameAndPass[i])) {
                return false;
            }
        }
    }

    return true;
    
}
