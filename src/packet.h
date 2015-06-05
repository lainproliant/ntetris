#pragma once

#include <stdint.h>
#include <uv.h>
#include <stddef.h>
#include <stdbool.h>

#define PROTOCOL_VERSION 0

typedef enum _MSG_TYPE {
    REGISTER_TETRAD, /* A new tetrad is in play */
    REGISTER_CLIENT, /* A new client has joined the server */
    UPDATE_TETRAD, /* The server updated a tetrad position */
    UPDATE_CLIENT_STATE, /* The server updated client stats & screen */
    DISCONNECT_CLIENT, /* The client disconnected from the server */
    KICK_CLIENT, /* The client was kicked from the server */
    CREATE_ROOM, /* A new game is created, with up to 4 boards */
    LIST_ROOMS, /* List the available rooms to join */
    USER_ACTION, /* A client is sending an action to be parsed */
    ERR_PACKET, /* There was an error parsing the client's packet */
    REG_ACK, /* Client registration acknowledgement */
    NUM_MESSAGES
} MSG_TYPE;

typedef enum _KICK_STATUS {
    KICKED,
    BANNED,
    NUM_KICK_STATUSES
} KICK_STATUS;

typedef enum _USER_CMD {
   ROTCW = 0,
   ROTCCW = 1,
   LOWER = 2,
   NUM_CMDS
} USER_CMD;

typedef enum _ERROR_CODE {
   UNSUPPORTED_MSG = 1, /* Message with unknown type */
   ILLEGAL_MSG = 2, /* Illegal message sent to server */
   BAD_LEN = 3, /* Invalid packet length */
   BAD_PROTOCOL = 4, /* Protocol version not PROTOCOL_VERSION */
   SUCCESS = 5, /* Just here for debugging the replies, will remove */
   NUM_ERR_CODES
} ERR_CODE;

/* This makes a bit more sense than a TLV, since L is only a field
 * needed for variable length packets. */
#pragma pack(1)
typedef struct _packet_t {
    uint8_t version; /* ABSOLUTE, for now always equals 0 */
    uint8_t type; /* the type of message */
    uint8_t data[];  /* the raw bytes to marshall and demarshall */
} packet_t;
#pragma pop

#pragma pack(1)
typedef struct _msg_err {
    uint8_t badMsgCode;
} msg_err;
#pragma pop

#pragma pack(1)
/* This message indicates the next tetrad(s) queued to drop */
typedef struct _msg_register_tetrad {
    uint8_t shape; /* All other fields are initialized the same */
} msg_register_tetrad;
#pragma pop

#pragma pack(1)
typedef struct _msg_register_client {
    uint8_t nameLength;
    char name[];
} msg_register_client;
#pragma pop

#pragma pack(1)
typedef struct _msg_update_tetrad {
    int x, y;
    int x0, y0;
    int rot;
} msg_update_tetrad;
#pragma pop

#pragma pack(1)
typedef struct _msg_update_client_state {
    int nlines; /* The number of lines cleared by the player */
    int score; /* The current score of the player */
    int level; /* The current difficulty level for the player */
    uint8_t status; /* the current game status */
    uint8_t nLinesChanged; /* The size of eliminated lines */
    uint16_t changedLines[]; /* Which lines to eliminate */
} msg_update_client_state;
#pragma pop

#pragma pack(1)
typedef struct _msg_create_room {
    uint8_t numPlayers;
    uint8_t roomNameLen;
    uint8_t passLen;
    char roomNameAndPass[];
} msg_create_room;
#pragma pop

#pragma pack(1)
typedef struct _msg_reg_ack {
    uint32_t curPlayerId;
} msg_reg_ack;
#pragma pop

#pragma pack(1)
typedef struct _msg_user_action {
    uint8_t cmd;
} msg_user_action;
#pragma pop

#pragma pack(1)
typedef struct _msg_kick_client {
    uint8_t kickStatus;
    uint16_t reasonLength;
    char reason[];
} msg_kick_client;
#pragma pop

void createErrPacket(packet_t *buf, ERR_CODE e);
void reply(packet_t *p, size_t psize, const struct sockaddr *from, int sock);
void setProtocolVers(packet_t *p);

/* This is meant to be used by both client & server */
bool validateLength(packet_t *p, ssize_t len, MSG_TYPE t, ssize_t *expectedSize);

/* These are the base sizes of each type, without the variable components 
 * packed on to the ends */
#define ERRMSG_SIZE sizeof(packet_t)+sizeof(msg_err) 
