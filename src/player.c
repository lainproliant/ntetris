#include "player.h"
#include "packet.h"
#include "macros.h"
#include <stdlib.h>
#include <stdio.h>
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

    return p;
}

void destroyPlayer(player_t *p)
{
    PRINT("Destroying player object[%p] (%s)\n", p, p->name);
    free(p->name);
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
        g_hash_table_remove(g_server->playersById, GUINT_TO_POINTER(p->playerId));
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
        g_hash_table_remove(g_server->playersById, GINT_TO_POINTER(p->playerId));
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
