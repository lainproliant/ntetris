#pragma once

#include <stdint.h>
#include <uv.h>

typedef enum _MSG_TYPE {
    REGISTER_TETRAD,
    REGISTER_CLIENT,
    UPDATE_TETRAD,
    UPDATE_CLIENT_STATE,
    DISCONNECT_CLIENT,
    KICK_CLIENT,
    CREATE_ROOM,
    USER_ACTION,
    ERR_PACKET,
    NUM_MESSAGES
} MSG_TYPE;

typedef enum _USER_CMD {
   ROTCW = 0,
   ROTCCW = 1,
   LOWER = 2
} USER_CMD;

typedef enum _ERROR_CODE {
   UNSUPPORTED_MSG = 1,
   NUM_ERR_CODES
} ERR_CODE;

typedef struct _TLV {
    uint8_t type;
    uint16_t length;
    uint8_t value[];
} TLV;

typedef struct _msg_err {
    uint8_t badMsgCode;
} msg_err;

typedef struct _msg_register_tetrad {
   int blah; 
} msg_register_tetrad;

typedef struct _msg_register_client {
    uint8_t nameLength;
    unsigned char name[];
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
    uint16_t changedLines[];
} msg_update_client_state;

typedef struct _msg_create_room {
    uint8_t numPlayers;
    uint8_t roomNameLen;
    unsigned char roomName[];
} msg_create_room;

typedef struct _msg_user_action {
    uint8_t cmd;
} msg_user_action;

void createErrPacket(TLV *buf, ERR_CODE e);
void reply(TLV *packet, uv_udp_t *req, const struct sockaddr *from);

#define ERRMSG_SIZE sizeof(TLV) + sizeof(msg_err) - 1
