#pragma once
#ifndef __ROOM_H
#define __ROOM_H

#include <glib.h>
#include <sys/socket.h>

struct _msg_create_room;

typedef struct _room_t {
    char *password;
    char *name;
    unsigned int id;
    GSList *players;
    uint8_t numPlayers;
} room_t;

room_t *createRoom(struct _msg_create_room *m, unsigned int id);
void destroyRoom(room_t *r, const char *optionalMsg);
unsigned int genRoomId(GHashTable *roomsById);
void announceRooms(const struct sockaddr *from);

#endif
