#include "packet.h"
#include "player.h"
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

void sendKickPacket(player_t *p, const char *reason, int sock)
{
    packet_t *m = NULL;
    msg_kick_client *mcast = NULL;

    if (reason != NULL) {
        m = malloc(sizeof(packet_t) + sizeof(msg_kick_client) + 
                    strlen(reason));
        mcast = (msg_kick_client*)m->data;
        mcast->reasonLength = htons(strlen(reason));
        memcpy(mcast->reason, reason, strlen(reason));
    } else {
        m = malloc(sizeof(packet_t) + sizeof(msg_kick_client));
        mcast = (msg_kick_client*)m->data;
        mcast->reasonLength = 0;
    }

    mcast->kickStatus = KICKED;
    reply(m, sizeof(m), &p->playerAddr, sock);
    free(m);
}

void handle_msg(uv_work_t *req)
{
    /* This function dispatches a request */
    request *r = req->data;
    packet_t *pkt = r->payload;

    /* Create a blank errPkt, populate later with 
     * createErrPacket */
    MSG_TYPE packetType = (MSG_TYPE)pkt->type;
    msg_err errMsg;
    msg_register_client *rclient = NULL;
    msg_create_room *croom = NULL;
    char *clientName = NULL;
    player_t *newPlayer = NULL;
    msg_reg_ack m_ack;
    msg_reg_ack *m_recAck = NULL;
    msg_ping *m_recPing = NULL;
    char senderIP[20] = { 0 };

    /* Stack allocated buffer for the error message packet */
    uint8_t errPktBuf[ERRMSG_SIZE];
    uint8_t ackPackBuf[sizeof(packet_t) + sizeof(msg_reg_ack)];
    packet_t *errPkt = (packet_t*)errPktBuf;
    packet_t *ackPack = (packet_t*)ackPackBuf;
    ssize_t ePktSize = -1;

    if ( PROTOCOL_VERSION != pkt->version ) {
        WARNING("Incorrect/unexpected protocol version\n"
                "Expected version %d, got %d :(", 
                PROTOCOL_VERSION, pkt->version);
        createErrPacket(errPkt, BAD_PROTOCOL);
        reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
        return;
    }

    /* Validate lengths */
    if(!validateLength((packet_t*)r->payload, r->len, packetType, &ePktSize)) {
        WARNING("Incorrect/unexpected packet length\n"
                "Expected %zd bytes, got %zd bytes", ePktSize, r->len);
        createErrPacket(errPkt, BAD_LEN);
        reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
        return;
    }

    switch(packetType) {
        case REGISTER_TETRAD:
        case ERR_PACKET:
        case KICK_CLIENT:
        case UPDATE_CLIENT_STATE:
            createErrPacket(errPkt, ILLEGAL_MSG);
            reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
            return;
            break;

        case REGISTER_CLIENT:
            rclient = (msg_register_client*)(pkt->data);

            if (!validateName(rclient)) {
                WARN("Non-printable or name length too long!");
                createErrPacket(errPkt, BAD_NAME);
                reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
                return;
            }

            /* This won't be NULL terminated */
            clientName = malloc(rclient->nameLength + 1);
            strlcpy(clientName, rclient->name, rclient->nameLength + 1);

            /* If this far, name meets requirements, now check for collisions */
            uv_rwlock_rdlock(playerTableLock);

            /* If there's no player with that name
             * Note: contains is probably a faster function for this but Oracle
             * and illumos both do not distribute very modern versions of glib
             * which contain these variants */
            if (!g_hash_table_lookup(playersByNames, clientName)) {
                uv_rwlock_rdunlock(playerTableLock);
                
                /* Make sure nothing can read or write to this but us */
                uv_rwlock_wrlock(playerTableLock);

                if (g_hash_table_lookup(playersByNames, clientName)) {
                    WARN("Somebody took our name while waiting for lock");
                    uv_rwlock_wrunlock(playerTableLock);
                    goto name_collide;
                }

                newPlayer = createPlayer(clientName, r->from,
                                            genPlayerId(playersById));
                g_hash_table_insert(playersByNames, clientName, newPlayer);
                g_hash_table_insert(playersById,
                                    GINT_TO_POINTER(newPlayer->playerId), 
                                    newPlayer);

                uv_rwlock_wrunlock(playerTableLock);

                PRINT("Registering client %s\n", newPlayer->name);
                m_ack.curPlayerId = htonl(newPlayer->playerId);
                ackPack->type = REG_ACK;
                memcpy(ackPack->data, &m_ack, sizeof(msg_reg_ack));
                reply(ackPack, sizeof(ackPackBuf), &r->from, vanillaSock);

            } else { /* Name collision, send back error */
                uv_rwlock_rdunlock(playerTableLock);
name_collide:
                WARNING("Name collision for %s", clientName);
                free(clientName);
                createErrPacket(errPkt, BAD_NAME);
                reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
                return;
            }

            return;
            break;

        case CREATE_ROOM:
            croom = (msg_create_room*)(pkt->data);
            
            if(!validateRoomName(croom)) {
                uv_ip4_name((const struct sockaddr_in*)&r->from, senderIP, 19);
                WARN("Non-printable or too long room name from %s!", senderIP);
                createErrPacket(errPkt, BAD_NAME);
                reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
                return;
            }

            break;

        case REG_ACK:
            m_recAck = (msg_reg_ack*)(pkt->data);
            regPlayer(m_recAck, &r->from);
            break;

        case PING:
            m_recPing = (msg_ping*)(pkt->data);
            pulsePlayer(m_recPing, &r->from);
            break;


        default:
            WARN("Unhandled packet type!!!!");
            createErrPacket(errPkt, UNSUPPORTED_MSG);
            reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
            return;
            break;
    }

}
