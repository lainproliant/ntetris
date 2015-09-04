#pragma once
#ifndef __ROOM_H
#define __ROOM_H

#include "player.h"
#include "packet.h"
#include <glib.h>

typedef struct _room_t {
    char *password;
    char *name;
    unsigned int id;
    GSList *players;
    uint8_t numPlayers;
} room_t;

room_t *createRoom(msg_create_room *m, unsigned int id);
void destroyRoom(room_t *r, const char *optionalMsg);
unsigned int genRoomId(GHashTable *roomsById);

#endif
