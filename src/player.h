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
struct _request;

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
} player_t;

player_t *createPlayer(const char *name, struct sockaddr sock, unsigned int id);
void destroyPlayer(player_t *p);
uint32_t genPlayerId(GHashTable *playersById);
void pulsePlayer(struct _msg_ping *m, const struct sockaddr *from);
void regPlayer(struct _msg_reg_ack *m, const struct sockaddr *from);
void kickPlayerByName(const char *name);
void kickPlayerById(unsigned int id, const char *reason);
void disconnectPlayer(uint32_t id, struct _request *r);
void printPlayer(gpointer k, gpointer v, gpointer d);
void printPlayers();
bool authPlayerPkt(player_t *p, const struct sockaddr *from, 
                   PLAYER_STATE min, PLAYER_STATE max);
bool validateName(struct _msg_register_client *m);

#endif
