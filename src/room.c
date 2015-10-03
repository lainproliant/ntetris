#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <uv.h>
#include "rand.h"
#include "player.h"
#include "packet.h"
#include "macros.h"
#include "room.h"

#ifdef __linux__
#include <bsd/stdlib.h>
#include <bsd/string.h>
#endif

packet_t *createRoomAnnouncement(room_t *r, size_t *packSize)
{
    /* These will vary in size, so have to dynamically allocate them */ 
    size_t packetSize = sizeof(msg_room_announce) + sizeof(packet_t) + 
                            strlen(r->name);
    packet_t *retVal = (packet_t*)malloc(packetSize);
    retVal->type = ROOM_ANNOUNCE;
    msg_room_announce *msg = (msg_room_announce*)(&retVal->data);
    msg->roomId = htonl(r->id);
    msg->numPlayers = r->numPlayers;

    /* Casting is safe here because it will never be > 4 */
    msg->numJoinedPlayers = (uint8_t)g_slist_length(r->players);

    msg->passwordProtected = (uint8_t)(r->password != NULL);
    msg->roomNameLen = strlen(r->name);

    memcpy(msg->roomName, r->name, msg->roomNameLen);

    *packSize = packetSize;

    return retVal;
}

room_t *createRoom(msg_create_room *m, unsigned int id)
{
    room_t *r = calloc(1, sizeof(room_t));
    r->name = calloc(m->roomNameLen + 1, sizeof(char));
    uv_rwlock_init(&r->roomLock);

    strlcpy(r->name, m->roomNameAndPass, m->roomNameLen + 1);

    if (m->passLen > 0) {
        r->password = calloc(m->passLen, sizeof(char));
        strlcpy(r->password, m->roomNameAndPass + m->roomNameLen, 
                m->passLen + 1); 
    }

    r->state = WAITING_FOR_PLAYERS;
    r->numPlayers = m->numPlayers;
    r->id = id;

    player_t *creator = NULL;
    GETPBYID(m->playerId, creator);

    uv_rwlock_wrlock(&creator->playerLock);
    r->players = g_slist_prepend(r->players, creator);
    creator->state = JOINED_AND_WAITING;
    creator->curJoinedRoomId = id;
    uv_rwlock_wrunlock(&creator->playerLock);

    return r;
}

void kickPlayerFromRoom(gpointer p, gpointer msg)
{
    player_t *player = (player_t*)p;
    const char *kmsg = (const char*)msg;
    sendKickPacket(p, kmsg, g_server->listenSock);

    destroyPlayer(p);
}

void destroyRoom(room_t *r, const char *optionalMsg)
{
    uv_rwlock_wrlock(&r->roomLock);
    g_slist_foreach(r->players, (GFunc)kickPlayerFromRoom, 
                    (gpointer)optionalMsg);
    g_slist_free(r->players);

    free(r->name);
    free(r->password);
    uv_rwlock_wrunlock(&r->roomLock);
    uv_rwlock_destroy(&r->roomLock);

    free(r); 
}

int joinPlayer(msg_join_room *m, player_t *p, room_t *r, 
                const struct sockaddr *from)
{
    /* This function returns an error code, otherwise returns 0 */
    if (r->password != NULL) {
        /* Server has a password, make sure player entered one */ 
        if (m->passwordLen != strlen(r->password) || 
            memcmp(m->password, r->password, m->passwordLen)) {
           return BAD_PASSWORD;
        }
    }

    uv_rwlock_wrlock(&r->roomLock);

    if (r->state != WAITING_FOR_PLAYERS) {
        return ROOM_FULL;
    }

    uv_rwlock_wrlock(&p->playerLock);
    r->players = g_slist_prepend(r->players, p);
    p->curJoinedRoomId = r->id;

    if (g_slist_length(r->players) == r->numPlayers) {
        r->state = IN_PROGRESS;
        p->state = PLAYING_GAME;
        /* Logic goes here for sending packets to inform players of 
         * state change */
    } else {
        p->state = JOINED_AND_WAITING;
    }

    uv_rwlock_wrunlock(&p->playerLock);
    uv_rwlock_wrunlock(&r->roomLock);
    
    return NULL;
}

void gh_announceRoom(gpointer k, gpointer v, gpointer d)
{
    room_t *r = (room_t*)v; 
    uint32_t roomId = GPOINTER_TO_UINT(k);
    packet_t *p = NULL;
    msg_room_announce *m = NULL;
    size_t size;

    const struct sockaddr *recipient = (const struct sockaddr*)d;

    if (r->state == WAITING_FOR_PLAYERS) {
       p = createRoomAnnouncement(r, &size);
       reply(p, size, recipient, g_server->listenSock); 
       m = (msg_room_announce*)p->data; 
       free(p);
    }
}

void announceRooms(const struct sockaddr *from)
{
    uv_rwlock_rdlock(g_server->roomsLock);
    g_hash_table_foreach(g_server->roomsById,
                         (GHFunc)gh_announceRoom,
                         (gpointer)from);
    uv_rwlock_rdunlock(g_server->roomsLock);
}

bool validateRoomName(msg_create_room *m)
{
    size_t i;

    if (m->roomNameLen > MAX_NAMELEN || m->roomNameLen == 0) {
        return false;
    } 

    for (i = 0; i < m->roomNameLen; ++i) {
        if (!isprint(m->roomNameAndPass[i])) {
            return false;
        }
    }

    return true;
}

void printRoom(gpointer k, gpointer v, gpointer d)
{
    room_t *r = (room_t*)v;
    size_t numPlayers = g_slist_length(r->players);
     
    PRINT("%-30s %-17u %10u / %d %-8d\n", r->name, r->id, 
            numPlayers, r->numPlayers, r->state);
}

void printRooms()
{
    uv_rwlock_rdlock(g_server->roomsLock);
    size_t numRooms = g_hash_table_size(g_server->roomsById);

    if (numRooms == 0) {
        WARN("[0 rooms found]");
    } else {
        PRINT("RoomList (%lu rooms) = \n", numRooms);
        PRINT(BOLDGREEN "%-30s %-17s %-10s %-8s\n",
             "Room Name", "RoomId", "Players Joined", "Room State");
        PRINT("-----------------------------------------"
              "---------------------------------------\n" RESET);
        g_hash_table_foreach(g_server->roomsById,
            (GHFunc)printRoom, NULL);
    }

    uv_rwlock_rdunlock(g_server->roomsLock);
}
