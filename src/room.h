#pragma once
#ifndef __ROOM_H
#define __ROOM_H

#include <glib.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <uv.h>
#include "player.h"
#include "state.h"

#define MIN_PLAYERS 2
#define MAX_PLAYERS 4
#define AOT_BLOCKS 4096

struct _msg_create_room;
struct _packet_t;
struct _msg_chat_msg;

typedef enum _ROOM_STATE {
    WAITING_FOR_PLAYERS,
    IN_PROGRESS,
    NUM_ROOM_STATES
} ROOM_STATE;

typedef struct _room_t {
    uv_rwlock_t roomLock; /* A more granular lock on a room */
    char *password; /* the password for the room */
    char *name; /* the name of the room */
    ROOM_STATE state; /* The state of the room */
    unsigned int id; /* the room id */
    player_t *players[MAX_PLAYERS]; /* Total joined players */
    uint32_t *publicIds; /* a list of player public ids per player */
    uint8_t *entropyPool; /* a large block of random bytes for blocks */
    uint8_t numPlayers; /* maximum number of players */
    STATE **gameStates; /* A list of the game states */
} room_t;

room_t *createRoom(struct _msg_create_room *m, unsigned int id);
void destroyRoom(room_t *r, const char *optionalMsg);
unsigned int genRoomId(GHashTable *roomsById);
void announceRooms(const struct sockaddr *from);
struct _packet_t *createRoomAnnouncement(room_t *r, size_t *packSize);
void addPlayer(room_t *r, player_t *p);
void removePlayer(room_t *r, player_t *p);
int getNumJoinedPlayers(room_t *r);
bool validateRoomName(struct _msg_create_room *m);
int joinPlayer(struct _msg_join_room *m, struct _player_t *p, 
               room_t *r, const struct sockaddr *from);
void printRooms();
void printRoom(gpointer k, gpointer v, gpointer d);
void sendChatMsg(player_t *p, room_t *r, struct _packet_t *m);

#ifdef DEBUG
/* locks room struct for reading */
#define readLockRoom(r) \
    do { \
       WARNING("read locking room %s", r->name);\
       uv_rwlock_rdlock(&r->roomLock);\
    } while (0)

/* unlocks player struct after reading */
#define readUnlockRoom(r) \
    do { \
       WARNING("read unlocking room %s", r->name);\
       uv_rwlock_rdunlock(&r->roomLock);\
    } while (0)

/* locks player struct for writing */
#define writeLockRoom(r) \
    do { \
       WARNING("write locking room %s", r->name);\
       uv_rwlock_wrlock(&r->roomLock);\
    } while (0)

/* unlocks player struct after writing */
#define writeUnlockRoom(r) \
    do { \
       WARNING("write unlocking room %s", r->name);\
       uv_rwlock_wrunlock(&r->roomLock);\
    } while (0)
#else
/* locks player struct for reading */
#define readLockRoom(r) uv_rwlock_rdlock(&r->roomLock);

/* unlocks player struct after reading */
#define readUnlockRoom(r) uv_rwlock_rdunlock(&r->roomLock);

/* locks player struct for writing */
#define writeLockRoom(r) uv_rwlock_wrlock(&r->roomLock);
       
/* unlocks player struct after writing */
#define writeUnlockRoom(r) uv_rwlock_wrunlock(&r->roomLock);\

#endif

#endif
