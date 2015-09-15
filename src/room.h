#pragma once
#ifndef __ROOM_H
#define __ROOM_H

#include <glib.h>
#include <sys/socket.h>
#include <stdbool.h>

struct _msg_create_room;
struct _packet_t;
struct _msg_create_room;

typedef enum _ROOM_STATE {
    WAITING_FOR_PLAYERS,
    IN_PROGRESS,
    NUM_ROOM_STATES
} ROOM_STATE;

typedef struct _room_t {
    char *password;
    char *name;
    ROOM_STATE state;
    unsigned int id;
    GSList *players;
    uint8_t numPlayers;
} room_t;

room_t *createRoom(struct _msg_create_room *m, unsigned int id);
void destroyRoom(room_t *r, const char *optionalMsg);
unsigned int genRoomId(GHashTable *roomsById);
void announceRooms(const struct sockaddr *from);
struct _packet_t *createRoomAnnouncement(room_t *r, size_t *packSize);
bool validateRoomName(struct _msg_create_room *m);
void printRooms();
void printRoom(gpointer k, gpointer v, gpointer d);

#endif
