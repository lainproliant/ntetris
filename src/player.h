#pragma once

/* These represent all the various possible states the player
 * can be represented by in the somewhat simple game state machine */
typedef enum _PLAYER_STATE {
    AWAITING_CLIENT_ACK, /* Waiting to validate */
    BROWSING_ROOMS, /* Client requested rooms list */
    JOINED_AND_WAITING, /* Client joined room and waiting for players */
    PLAYING_GAME, /* Client is playing a game */
    NUM_STATES
} PLAYER_STATE;

typedef struct _player_t {
    uint64_t lastpkt_ts; /* The ms/ns timestamp of last command packet */ 
    int32_t secBeforeNextPingMax; /* max seconds before next ping */
    uint32_t playerId;  /* The unique playerId */
    uint32_t curJoinedRoomId; /* The room the player is in */
    struct sockaddr playerAddr; /* Their IP + port combo */
    PLAYER_STATE state;
    char *name; /* The player's name */
} player_t;

player_t *createPlayer(const char *name, struct sockaddr sock);
void *destroyPlayer(player_t *p);
