#include "player.h"
#include "packet.h"
#include "room.h"
#include "macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include "rand.h"

#ifdef __linux__
#include <bsd/string.h>
#else
#include <string.h>
#endif

player_t *createPlayer(const char *name, struct sockaddr sock, unsigned int id)
{
    player_t *p = malloc(sizeof(player_t));
    p->playerId = id;

    /* name param is expected to not be NUL teriminated, this fixes that */
    p->name = (char*)name;
    PRINT("Creating player (%u, %s)\n", id, p->name);

    p->state = AWAITING_CLIENT_ACK;
    p->secBeforeNextPingMax = 40;
    p->curJoinedRoomId = 0;
    p->playerAddr = sock;

    uv_rwlock_init(&p->playerLock);

    return p;
}

void removePlayerFromRoom(player_t *p)
{
    uv_rwlock_wrlock(&p->playerLock);
    p->curJoinedRoomId = 0;
    p->state = BROWSING_ROOMS;
    p->publicId = 0;
    uv_rwlock_wrunlock(&p->playerLock);
}

void destroyPlayer(player_t *p)
{
    room_t *r = NULL;
    uv_rwlock_wrlock(&p->playerLock);
    PRINT("Destroying player object[%p] (%s)\n", p, p->name);

    if (p->state >= JOINED_AND_WAITING) {
       GETRBYID(p->curJoinedRoomId, r); 

        if (r != NULL) {
            /* If they are the final player */
            if (g_slist_length(r->players) == 1) {
                uv_rwlock_wrunlock(&p->playerLock);
                destroyRoom(r, "Final player quit");
                uv_rwlock_wrlock(&p->playerLock);
            } else {
                uv_rwlock_wrlock(&r->roomLock);
                /* TODO: Add logic here for notifying players
                 * in the room that this player has left. This
                 * will be a status update with game over / forfeit
                 * with the player's identifier (msg_update_client_state) */
                r->players = g_slist_remove(r->players, p);
                uv_rwlock_wrunlock(&r->roomLock);
            }
        }
    }

    free(p->name);
    uv_rwlock_wrunlock(&p->playerLock);
    uv_rwlock_destroy(&p->playerLock);
    free(p);
}

void pulsePlayer(msg_ping *m, const struct sockaddr *from)
{
    uint32_t playerId = ntohl(m->playerId);
    player_t *p = NULL;

    uv_rwlock_rdlock(g_server->playerTableLock);
    p = g_hash_table_lookup(g_server->playersById, GUINT_TO_POINTER(playerId));

    /* Not sure this is a great way to compare addr, hopefully padding
       isn't unitialized memory or something */
    if (authPlayerPkt(p, from, BROWSING_ROOMS, NUM_STATES)) {
        p->secBeforeNextPingMax = 40;
    } else {
        WARNING("Player with id %u not found or"
                " message was sent from invalid addr", playerId);
    }
    uv_rwlock_rdunlock(g_server->playerTableLock);
}

void regPlayer(msg_reg_ack *m, const struct sockaddr *from)
{
    uint32_t playerId = ntohl(m->curPlayerId);
    player_t *p = NULL;

    uv_rwlock_rdlock(g_server->playerTableLock);
    p = g_hash_table_lookup(g_server->playersById, GUINT_TO_POINTER(playerId));

    if (authPlayerPkt(p, from, AWAITING_CLIENT_ACK, AWAITING_CLIENT_ACK)) {
    /* Not sure this is a great way to compare addr, hopefully padding
       isn't unitialized memory or something */
        p->state = BROWSING_ROOMS; 
    } else {
        WARNING("Player with id %u not found or"
                " message was sent from invalid addr"
                " or player was in inconsistent state", playerId);
    }
    uv_rwlock_rdunlock(g_server->playerTableLock);
}

void kickPlayerByName(const char *name) 
{
    uv_rwlock_wrlock(g_server->playerTableLock);
    player_t *p = NULL;
    p = g_hash_table_lookup(g_server->playersByNames, name);

    if (p == NULL) {
        WARNING("Player %s not found", name);
    } else {
        // kick packet logic goes here 
        g_hash_table_remove(g_server->playersByNames, name);
        g_hash_table_remove(g_server->playersById, 
                            GUINT_TO_POINTER(p->playerId));
        PRINT("Kicked player [%u] (%s)\n", p->playerId, name);

        sendKickPacket(p, (const char*)NULL, g_server->listenSock);

        destroyPlayer(p);
    }

    uv_rwlock_wrunlock(g_server->playerTableLock);
}

void kickPlayerById(unsigned int id, const char *reason)
{
    uv_rwlock_wrlock(g_server->playerTableLock);
    player_t *p = NULL;
    p = g_hash_table_lookup(g_server->playersById, GUINT_TO_POINTER(id));

    if (p == NULL) {
        WARNING("Player id: %u not found", id);
    } else {
        g_hash_table_remove(g_server->playersByNames, p->name);
        g_hash_table_remove(g_server->playersById, 
                            GINT_TO_POINTER(p->playerId));
        PRINT("Kicked player [%u] (%s)\n", p->playerId, p->name);
        sendKickPacket(p, reason, g_server->listenSock);
        destroyPlayer(p);
    }

    uv_rwlock_wrunlock(g_server->playerTableLock);
}

void printPlayer(gpointer k, gpointer v, gpointer d)
{
    char ipBuf[20];
    player_t *p = (player_t*)v;
    uv_ip4_name((const struct sockaddr_in*)(&p->playerAddr), ipBuf, 19);
    PRINT("%-30s %-17u %-19s %-4d %-4d\n",
        p->name,
        p->playerId,
        ipBuf,
        p->secBeforeNextPingMax,
        p->state);
}

void printPlayers()
{
    uv_rwlock_rdlock(g_server->playerTableLock);
    size_t numPlayers = g_hash_table_size(g_server->playersById);

    if (numPlayers == 0) {
        WARN("[0 players found]");
    } else {
        PRINT("PlayerList (%lu players) = \n", numPlayers);
        PRINT(BOLDGREEN "%-30s %-17s %-19s %-4s %-4s\n", 
            "Player Name", "PlayerId",\
            "Player IP", "TTL", "STATE");
        PRINT("-----------------------------------------"
              "---------------------------------------\n" RESET);
        g_hash_table_foreach(g_server->playersById, 
            (GHFunc)printPlayer, NULL);
    }
    
    uv_rwlock_rdunlock(g_server->playerTableLock);
}

void disconnectPlayer(uint32_t id, request *r)
{
    player_t *p = NULL; 
    const struct sockaddr *from = &r->from;
    char ipBuf[20];
    uv_ip4_name((const struct sockaddr_in*)(from), ipBuf, 19);
    GETPBYID(id, p);

    if (authPlayerPkt(p, from, BROWSING_ROOMS, NUM_STATES)) {
        kickPlayerById(id, "Client requested disconnect");
    } else {
        WARNING("Player id %u does not match IP established (%s)!", 
                id, ipBuf);
    }
}

bool authPlayerPkt(player_t *p, const struct sockaddr *from,
                   PLAYER_STATE min, PLAYER_STATE max)
{
    if (p != NULL && 
        (!memcmp(p->playerAddr.sa_data, from->sa_data, sizeof(from->sa_data)) &&
        min == NUM_STATES || (p->state >= min && p->state <= max))) {
        return true;
    }

    return false;
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

struct playerMsgRoomTuple {
    size_t msgSize;
    packet_t *msg; /* The announcement message */
    player_t *p; /* The player whose lock to exclude */
    room_t *r; /* The room we are announcing to */
};

void gl_sendPlayerAnnouncement(gpointer v, gpointer d)
{
    player_t *p = (player_t*)v;
    struct playerMsgRoomTuple *pmrt = (struct playerMsgRoomTuple*)d;

    if (v != pmrt->p) {
        uv_rwlock_rdlock(&p->playerLock);
    }

    reply(pmrt->msg, pmrt->msgSize, &p->playerAddr, g_server->listenSock);

    if (v != pmrt->p) {
        uv_rwlock_rdunlock(&p->playerLock);
    }
}

void gl_announcePlayer(gpointer v, gpointer d)
{
    player_t *p = (player_t*)v;
    msg_opponent_announce *msg = NULL;
    packet_t *moa = NULL;
    uint32_t pubId;
    struct playerMsgRoomTuple *pmrt = (struct playerMsgRoomTuple*)d;
    GSList *playerCursor = pmrt->r->players;
    int playerSlot = 0;
    size_t msgSize = 0;

    if (p != pmrt->p) {
        uv_rwlock_rdlock(&p->playerLock);
    }

    while (playerCursor->data != pmrt->p && playerCursor != NULL) {
        playerCursor = playerCursor->next;
        ++playerSlot;
    }

    /* May move this to a function */
    msgSize = sizeof(packet_t) + sizeof(msg_opponent_announce) +
              strlen(p->name);
    moa = malloc(msgSize);

    moa->type = OPPONENT_ANNOUNCE;
    msg = (msg_opponent_announce*)moa->data;
    msg->playerPubId = pubId;

    pmrt->msg = moa;
    pmrt->msgSize = msgSize;

    /* We reached end of list, couldn't find player, assume something wrong */
    if (playerCursor == NULL) goto error;

    pubId = pmrt->r->publicIds[playerSlot];
    /* Inform every player of every player */
     
    msg->playerPubId = pubId;
    msg->nameLength = strlen(p->name);
    memcpy(msg->playerName, p->name, msg->nameLength);

    if (p != pmrt->p) {
        uv_rwlock_rdunlock(&p->playerLock);
    }
    
    g_slist_foreach(pmrt->r->players, (GFunc)gl_sendPlayerAnnouncement, pmrt);

error:
    free(moa);
    return;
}

void announcePlayer(player_t *p, room_t *r)
{
    msg_opponent_announce *msg = NULL;
    struct playerMsgRoomTuple pmrt;
    pmrt.msg = NULL; 
    pmrt.r = r;
    pmrt.p = p;

    g_slist_foreach(r->players, (GFunc)gl_announcePlayer, (gpointer)&pmrt);
}
