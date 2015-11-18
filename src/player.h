#pragma once
#ifndef __PLAYER_H
#define __PLAYER_H

#include <sys/socket.h>
#include <stdint.h>
#include <glib.h>
#include <stdint.h>
#include <uv.h>
#include <stdbool.h>
#include "server.h"

/* Use forward declarations instead, can prevent some later headaches */
struct _msg_ping;
struct _msg_reg_ack;
struct _msg_register_client;
struct _room_t;
struct _request;

typedef enum _JOIN_STATUS {
    PLAYER_JOINED = 0,
    PLAYER_LEFT = 1,
    NUM_STATUSES
} JOIN_STATUS;

extern server_t *g_server;

/* These represent all the various possible states the player
 * can be represented by in the somewhat simple game state machine.
 * It's possible to ++ the enum to advance to the next state but
 * I'm not sure that this is very explicit or obvious */
typedef enum _PLAYER_STATE {
    AWAITING_CLIENT_ACK, /* Waiting to validate */
    BROWSING_ROOMS, /* Client requested rooms list */
    JOINED_AND_WAITING, /* Client joined room and waiting for players */
    PLAYING_GAME, /* Client is playing a game */
    NUM_STATES
} PLAYER_STATE;

typedef struct _player_t {
    uint64_t lastpkt_ts; /* The ms/ns timestamp of last command packet */ 
    int secBeforeNextPingMax; /* max seconds before next ping */
    unsigned int playerId;  /* The unique playerId */
    unsigned int curJoinedRoomId; /* The room the player is in */
    struct sockaddr playerAddr; /* Their IP + port combo */
    PLAYER_STATE state; /* We could ++ to advance to next state */
    char *name; /* The player's name */
    uint32_t publicId; /* Used by other players in the same room */
    uv_rwlock_t playerLock; /* a more granular lock for the player */
} player_t;

/* Instantiate the player object */
player_t *createPlayer(const char *name, struct sockaddr sock, unsigned int id);

/* free the player */
void destroyPlayer(player_t *p);

/* Create a unique playerId */
uint32_t genPlayerId(GHashTable *playersById);

/* Player has sent a ping, reset their timeout counter */
void pulsePlayer(struct _msg_ping *m, const struct sockaddr *from);

/* Register the player */
void regPlayer(struct _msg_reg_ack *m, const struct sockaddr *from);

/* Kick the player by their name */
void kickPlayerByName(const char *name);

/* Kick the player by their given id, with optional reason */
void kickPlayerById(unsigned int id, const char *reason);

/* disconnect and kick the player */
void disconnectPlayer(uint32_t id, struct _request *r);

/* Work function for a foreach ghashtable */
void printPlayer(gpointer k, gpointer v, gpointer d);

/* Administrative function for listing players */
void printPlayers();

/* Makes sure the player is in the correct state in the game state
 * machine and that they are in fact who they say they are */
bool authPlayerPkt(player_t *p, const struct sockaddr *from, 
                   PLAYER_STATE min, PLAYER_STATE max);

/* Validates the name is printable and a valid length */
bool validateName(struct _msg_register_client *m);

/* Removes the player from the room */
void removePlayerFromRoom(player_t *p);

/* Announces a successfully joined player p to room r */
void announcePlayer(player_t *p, struct _room_t *r, JOIN_STATUS js);

/* Announces successfully joined players to p for to room r */
void announceJoinedPlayers(player_t *p, struct _room_t *r);

#endif
