#pragma once

#include <sys/socket.h>

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
void *destroyPlayer(player_t *p);
