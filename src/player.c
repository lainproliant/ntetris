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

uint32_t genPlayerId(GHashTable *playersById)
{
    /* Find a non-colliding Id, probably will not require many loops */
    uint32_t retVal;

    do {
        retVal = getRand();
    } while (g_hash_table_lookup(playersById, GUINT_TO_POINTER(retVal)));

    return retVal;
}

void pulsePlayer(msg_ping *m, const struct sockaddr *from)
{
    uint32_t playerId = ntohl(m->playerId);
    player_t *p = NULL;

    uv_rwlock_rdlock(playerTableLock);
    p = g_hash_table_lookup(playersById, GUINT_TO_POINTER(playerId));

    /* Not sure this is a great way to compare addr, hopefully padding
       isn't unitialized memory or something */
    if (p != NULL && 
        !memcmp(p->playerAddr.sa_data, 
                from->sa_data,
                sizeof(from->sa_data))) {
        p->secBeforeNextPingMax = 40; 
    } else {
        WARNING("Player with id %u not found or"
                " message was sent from invalid addr", playerId);
    }
    uv_rwlock_rdunlock(playerTableLock);
}

void regPlayer(msg_reg_ack *m, const struct sockaddr *from)
{
    uint32_t playerId = ntohl(m->curPlayerId);
    player_t *p = NULL;

    uv_rwlock_rdlock(playerTableLock);
    p = g_hash_table_lookup(playersById, GUINT_TO_POINTER(playerId));

    /* Not sure this is a great way to compare addr, hopefully padding
       isn't unitialized memory or something */
    if (p != NULL && 
        !memcmp(p->playerAddr.sa_data, 
                from->sa_data,
                sizeof(from->sa_data)) &&
                p->state == AWAITING_CLIENT_ACK) {
        p->state = BROWSING_ROOMS; 
    } else {
        WARNING("Player with id %u not found or"
                " message was sent from invalid addr"
                " or player was in inconsistent state", playerId);
    }
    uv_rwlock_rdunlock(playerTableLock);
}

void kickPlayerByName(const char *name) 
{
    uv_rwlock_wrlock(playerTableLock);
    player_t *p = NULL;
    p = g_hash_table_lookup(playersByNames, name);

    if (p == NULL) {
        WARNING("Player %s not found", name);
    } else {
        // kick packet logic goes here 
        g_hash_table_remove(playersByNames, name);
        g_hash_table_remove(playersById, GUINT_TO_POINTER(p->playerId));
        PRINT("Kicked player [%u] (%s)\n", p->playerId, name);

        sendKickPacket(p, (const char*)NULL, vanillaSock);

        destroyPlayer(p);
    }

    uv_rwlock_wrunlock(playerTableLock);
}

void kickPlayerById(unsigned int id, const char *reason)
{
    uv_rwlock_wrlock(playerTableLock);
    player_t *p = NULL;
    p = g_hash_table_lookup(playersById, GUINT_TO_POINTER(id));

    if (p == NULL) {
        WARNING("Player id: %u not found", id);
    } else {
        g_hash_table_remove(playersByNames, p->name);
        g_hash_table_remove(playersById, GINT_TO_POINTER(p->playerId));
        PRINT("Kicked player [%u] (%s)\n", p->playerId, p->name);
        sendKickPacket(p, reason, vanillaSock);
        destroyPlayer(p);
    }

    uv_rwlock_wrunlock(playerTableLock);
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
    uv_rwlock_rdlock(playerTableLock);
    size_t numPlayers = g_hash_table_size(playersById);

    if (numPlayers == 0) {
        WARN("[0 players found]");
    } else {
        PRINT("PlayerList (%lu players) = \n", numPlayers);
        PRINT(BOLDGREEN "%-30s %-17s %-19s %-4s %-4s\n", 
            "Player Name", "PlayerId",\
            "Player IP", "TTL", "STATE");
        PRINT("-----------------------------------------"
              "---------------------------------------\n" RESET);
        g_hash_table_foreach(playersById, 
            (GHFunc)printPlayer, NULL);
    }
    
    uv_rwlock_rdunlock(playerTableLock);
}
