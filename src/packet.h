#pragma once

#include <stdint.h>

typedef enum _MSG_TYPE {
    REGISTER_TETRAD,
    REGISTER_CLIENT,
    UPDATE_TETRAD,
    UPDATE_CLIENT_STATE,
    DISCONNECT_CLIENT,
    KICK_CLIENT,
    CREATE_ROOM,
    NUM_MESSAGES
} MSG_TYPE;

typedef struct _TLV {
    uint8_t type;
    uint16_t length;
    uint8_t value[0];
} TLV;

typedef struct _msg_register_client {
    uint8_t nameLength;
    unsigned char name[0];
} msg_register_client;

typedef struct _msg_update_tetrad {
    int x, y;
    int x0, y0;
    int rot;
} msg_update_tetrad;

typedef struct _msg_update_client_state {
    int nlines;
    int score;
    int level;
    uint8_t status; 
    uint8_t nLinesChanged; 
    uint16_t changedLines[0];
} msg_update_client_state;

typedef struct _msg_create_room {
    uint8_t numPlayers;
    uint8_t roomNameLen;
    unsigned char roomName[0];
} msg_create_room;

msg_update_tetrad tetrad;
TLV *pTLV = malloc(sizeof(TLV) + sizeof(msg_update_tetrad));


