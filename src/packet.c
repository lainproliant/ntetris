#include "packet.h"
#include "rand.h"
#include "room.h"
#include "player.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "macros.h"
#include "game.h"
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#if defined(__linux__)
#include <bsd/stdlib.h>
#include <bsd/string.h>
#endif

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
    msg_chat_msg *chat = NULL;

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
        case CHAT:
            if (len < sizeof(msg_chat_msg) + totalBytes) {
                return false;
            }

            chat = (msg_chat_msg*)p->data;
            totalBytes += sizeof(msg_chat_msg) + \
                 ntohs(chat->msgLength) * sizeof(char);
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
                jroom->passwordLen * sizeof(char);
            break;
        case ROOM_ANNOUNCE:
            if (len < sizeof(msg_room_announce) + totalBytes) {
                return false;
            }

            aroom = (msg_room_announce*)p->data;
            totalBytes += sizeof(msg_room_announce) + \
                aroom->roomNameLen * sizeof(char);
            break;
        
        default:
            WARNING("unhandled type passed in (%d)", t);
            return false;
            break;
    }
            *expectedSize = totalBytes;
            return (len == totalBytes);
}

void sendKickPacket(player_t *p, const char *reason, int sock)
{
    packet_t *m = NULL;
    msg_kick_client *mcast = NULL;
    size_t packetSize = sizeof(packet_t);

    if (reason != NULL) {
        packetSize += sizeof(msg_kick_client) + strlen(reason);
        m = malloc(packetSize);
        m->type = KICK_CLIENT;
        mcast = (msg_kick_client*)m->data;
        mcast->reasonLength = htons(strlen(reason));
        memcpy(mcast->reason, reason, strlen(reason));
    } else {
        packetSize += sizeof(msg_kick_client);
        m = malloc(packetSize);
        m->type = KICK_CLIENT;
        mcast = (msg_kick_client*)m->data;
        mcast->reasonLength = 0;
    }

    mcast->kickStatus = KICKED;
    reply(m, packetSize, &p->playerAddr, sock);
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
    char *clientName = NULL;
    char *roomName = NULL;
    player_t *newPlayer = NULL;
    player_t *pkt_player = NULL;

    /* Different possible message types */
    msg_register_client *rclient = NULL;
    msg_create_room *croom = NULL;
    msg_reg_ack m_ack;
    msg_reg_ack *m_recAck = NULL;
    msg_ping *m_recPing = NULL;
    msg_disconnect_client *m_dc = NULL;
    msg_list_rooms *m_lr = NULL;
    msg_join_room *m_jr = NULL;
    msg_chat_msg *m_chat = NULL;
    msg_user_action *m_userAction = NULL;

    room_t *newRoom = NULL;
    room_t *joinedRoom = NULL;
    uint32_t incomingId;
    uint32_t incomingRId;
    ERR_CODE roomJoinRes;
    char senderIP[17] = { 0 };

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
        reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
        return;
    }

    /* Validate lengths */
    if(!validateLength((packet_t*)r->payload, r->len, packetType, &ePktSize)) {
        WARNING("Incorrect/unexpected packet length\n"
                "Expected %zd bytes, got %zd bytes", ePktSize, r->len);
        createErrPacket(errPkt, BAD_LEN);
        reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
        return;
    }

    switch(packetType) {
        case REGISTER_TETRAD:
        case ERR_PACKET:
        case KICK_CLIENT:
        case UPDATE_CLIENT_STATE:
        case UPDATE_TETRAD:
            createErrPacket(errPkt, ILLEGAL_MSG);
            reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
            return;
            break;

        case REGISTER_CLIENT:
            rclient = (msg_register_client*)(pkt->data);

            if (!validateName(rclient)) {
                WARN("Non-printable or name length too long!");
                createErrPacket(errPkt, BAD_NAME);
                reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
                return;
            }

            /* This won't be NULL terminated */
            clientName = malloc(rclient->nameLength + 1);
            strlcpy(clientName, rclient->name, rclient->nameLength + 1);

            /* If this far, name meets requirements, now check for collisions */
            uv_rwlock_rdlock(g_server->playerTableLock);

            /* If there's no player with that name
             * Note: contains is probably a faster function for this but Oracle
             * and illumos both do not distribute very modern versions of glib
             * which contain these variants */
            if (!g_hash_table_lookup(g_server->playersByNames, clientName)) {
                uv_rwlock_rdunlock(g_server->playerTableLock);
                
                /* Make sure nothing can read or write to this but us */
                uv_rwlock_wrlock(g_server->playerTableLock);

                if (g_hash_table_lookup(g_server->playersByNames, clientName)) {
                    WARN("Somebody took our name while waiting for lock");
                    uv_rwlock_wrunlock(g_server->playerTableLock);
                    goto name_collide;
                }

                newPlayer = createPlayer(clientName, r->from,
                                            genRandId(g_server->playersById));

                g_hash_table_insert(g_server->playersByNames, 
                                    clientName,
                                    newPlayer);

                g_hash_table_insert(g_server->playersById,
                                    GUINT_TO_POINTER(newPlayer->playerId), 
                                    newPlayer);

                uv_rwlock_wrunlock(g_server->playerTableLock);

                PRINT("Registering client %s\n", newPlayer->name);
                m_ack.curPlayerId = htonl(newPlayer->playerId);
                ackPack->type = REG_ACK;
                memcpy(ackPack->data, &m_ack, sizeof(msg_reg_ack));

                reply(ackPack, sizeof(ackPackBuf),
                      &r->from, g_server->listenSock);

            } else { /* Name collision, send back error */
                uv_rwlock_rdunlock(g_server->playerTableLock);
name_collide:
                WARNING("Name collision for %s", clientName);
                free(clientName);
                createErrPacket(errPkt, BAD_NAME);
                reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
                return;
            }

            return;
            break;

        case CREATE_ROOM:
            croom = (msg_create_room*)(pkt->data);
            
            if (!validateRoomName(croom)) {
                uv_ip4_name((const struct sockaddr_in*)&r->from, senderIP, 16);
                WARN("Non-printable or too long room name from %s!", senderIP);
                createErrPacket(errPkt, BAD_NAME);
                reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
                return;
            }

            incomingId = ntohl(croom->playerId);
            GETPBYID(incomingId, pkt_player);

            if (authPlayerPkt(pkt_player, &r->from, 
                    BROWSING_ROOMS, BROWSING_ROOMS)) {

                /* Check for a valid number of players */
                if (croom->numPlayers < MIN_PLAYERS || 
                    croom->numPlayers > MAX_PLAYERS) {
                    createErrPacket(errPkt, BAD_NUM_PLAYERS);
                    reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
                    return;
                }

                /* Check for a room name collision */
                roomName = malloc(croom->roomNameLen + 1);
                strlcpy(roomName, croom->roomNameAndPass, croom->roomNameLen+1);

                uv_rwlock_rdlock(g_server->roomsLock);

                if (!g_hash_table_lookup(g_server->roomsByName, roomName)) {
                    uv_rwlock_rdunlock(g_server->roomsLock);

                    uv_rwlock_wrlock(g_server->roomsLock);
                    if (g_hash_table_lookup(g_server->roomsByName,
                                             roomName)) {
                        WARN("Somebody stole room name while waiting for lock");
                        uv_rwlock_wrunlock(g_server->roomsLock);
                        goto room_name_collide;
                    }

                    newRoom = createRoom(croom, genRandId(g_server->roomsById));

                    g_hash_table_insert(g_server->roomsByName, 
                                        roomName, newRoom);

                    g_hash_table_insert(g_server->roomsById,
                                        GUINT_TO_POINTER(newRoom->id),
                                        newRoom);
                    uv_rwlock_wrunlock(g_server->roomsLock);
                } else {
                    uv_rwlock_rdunlock(g_server->roomsLock);
room_name_collide:
                    WARNING("Room name collision for %s", roomName);
                    free(roomName);
                    createErrPacket(errPkt, BAD_ROOM_NAME);
                    reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
                    return;
                }
            }

            return;
            break;

        case REG_ACK:
            m_recAck = (msg_reg_ack*)(pkt->data);
            regPlayer(m_recAck, &r->from);
            break;

        case USER_ACTION:
            m_userAction = (msg_user_action*)(pkt->data);
            incomingId = ntohl(m_userAction->playerId);
            GETPBYID(incomingId, pkt_player);
            if (authPlayerPkt(pkt_player, &r->from, 
                    PLAYING_GAME, PLAYING_GAME)) {
               parsePlayerAction(m_userAction, pkt_player);
            }
            
            break;

        case PING:
            m_recPing = (msg_ping*)(pkt->data);
            incomingId = ntohl(m_recPing->playerId);

            GETPBYID(incomingId, pkt_player);

            if (authPlayerPkt(pkt_player, &r->from,
                BROWSING_ROOMS, NUM_STATES)) {
                pulsePlayer(m_recPing, &r->from);
            }

            break;

        case DISCONNECT_CLIENT:
            m_dc = (msg_disconnect_client*)(pkt->data);
            disconnectPlayer(ntohl(m_dc->playerId), r);
            break;

        case LIST_ROOMS:
            m_lr = (msg_list_rooms*)(pkt->data);
            incomingId = ntohl(m_lr->playerId);
            
            GETPBYID(incomingId, pkt_player);

            if (authPlayerPkt(pkt_player, &r->from, 
                    BROWSING_ROOMS, BROWSING_ROOMS)) {
                PRINT(BOLDCYAN "PLAYER SUCCESSFULLY REQUESTED ROOMS!\n" RESET);
                announceRooms(&r->from);
            } else {
                uv_ip4_name((const struct sockaddr_in*)&r->from, senderIP, 16);
                WARNING("Player id(%u) / ip(%s) in packet is wrong or packet"
                        " is invalid for given player state", 
                        incomingId, senderIP);
            }
             
            return;
            break;
        case JOIN_ROOM:
            m_jr = (msg_join_room*)(pkt->data);
            incomingId = ntohl(m_jr->playerId);
            
            GETPBYID(incomingId, pkt_player);

            if (authPlayerPkt(pkt_player, &r->from,
                    BROWSING_ROOMS, BROWSING_ROOMS)) {
                incomingRId = ntohl(m_jr->roomId);

                GETRBYID(incomingRId, joinedRoom);

                if (joinedRoom == NULL) {
                    createErrPacket(errPkt, BAD_ROOM_NUM);
                    reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
                    return;
                }

                roomJoinRes = joinPlayer(m_jr, pkt_player, joinedRoom, &r->from);

                if (roomJoinRes != 0) {
                    createErrPacket(errPkt, roomJoinRes);
                    reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
                }

            } else {
                uv_ip4_name((const struct sockaddr_in*)&r->from, senderIP, 16);
                WARNING("Player id(%u) / ip(%s) in packet is wrong or packet"
                        " is invalid for given player state", 
                        incomingId, senderIP);
            }
            return;
            break;

        case CHAT:
            m_chat = (msg_chat_msg*)(pkt->data); 
            incomingId = ntohl(m_chat->playerId);

            GETPBYID(incomingId, pkt_player);

            if (authPlayerPkt(pkt_player, &r->from,
                   JOINED_AND_WAITING, NUM_STATES)) {
                incomingRId = pkt_player->curJoinedRoomId; 
                GETRBYID(incomingRId, joinedRoom);
                sendChatMsg(pkt_player, joinedRoom, pkt);
            } else {
                uv_ip4_name((const struct sockaddr_in*)&r->from, senderIP, 16);
                WARNING("Player id(%u) / ip(%s) in packet is wrong or packet"
                        " is invalid for given player state", 
                        incomingId, senderIP);
            }
            return;
            break;

        default:
            WARNING("Unhandled packet type!!!! (%d)", packetType);
            createErrPacket(errPkt, UNSUPPORTED_MSG);
            reply(errPkt, ERRMSG_SIZE, &r->from, g_server->listenSock);
            return;
            break;
    }

}
