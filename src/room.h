#pragma once
#ifndef __ROOM_H
#define __ROOM_H

#include <glib.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <uv.h>

struct _msg_create_room;
struct _packet_t;
struct _msg_create_room;

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
    GSList *players; /* Total joined players */
    uint8_t numPlayers; /* maximum number of players */
} room_t;

room_t *createRoom(struct _msg_create_room *m, unsigned int id);
void destroyRoom(room_t *r, const char *optionalMsg);
unsigned int genRoomId(GHashTable *roomsById);
void announceRooms(const struct sockaddr *from);
struct _packet_t *createRoomAnnouncement(room_t *r, size_t *packSize);
bool validateRoomName(struct _msg_create_room *m);
int joinPlayer(struct _msg_join_room *m, struct _player_t *p, 
               room_t *r, const struct sockaddr *from);
void printRooms();
void printRoom(gpointer k, gpointer v, gpointer d);

#endif
