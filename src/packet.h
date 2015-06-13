#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/socket.h>

#define PROTOCOL_VERSION 0
#define MAX_NAMELEN 30

typedef enum _MSG_TYPE {
    REGISTER_TETRAD, /* A new tetrad is in play */
    REGISTER_CLIENT, /* A new client has joined the server */
    UPDATE_TETRAD, /* The server updated a tetrad position */
    UPDATE_CLIENT_STATE, /* The server updated client stats & screen */
    DISCONNECT_CLIENT, /* The client disconnected from the server */
    KICK_CLIENT, /* The client was kicked from the server */
    CREATE_ROOM, /* A new game is created, with up to 4 boards */
    LIST_ROOMS, /* List the available rooms to join */
    ROOM_ANNOUNCE, /* Server sends client room / game info */
    JOIN_ROOM, /* Client picks a room to join */
    USER_ACTION, /* A client is sending an action to be parsed */
    ERR_PACKET, /* There was an error parsing the client's packet */
    REG_ACK, /* Client registration acknowledgement */
    PING, /* Client pings back to the server to prevent auto timeout */
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
   BAD_NAME = 5, /* The user's namestr is too long or stupid */
   BAD_ROOM_NAME = 6, /* The user room name is too long or stupid */
   SUCCESS = 7, /* Just here for debugging the replies, will remove */
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
    uint32_t playerId; /* Unique player identifier to basic auth */
    uint8_t numPlayers; /* The number of players to wait for */
    uint8_t roomNameLen; /* The length of the room string */
    uint8_t passLen; /* The length of the password string */
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
    uint32_t playerId; /* The unique playerId assigned via ack */
    uint8_t cmd; /* the command the player is sending to server */
} msg_user_action;
#pragma pop

#pragma pack(1)
typedef struct _msg_kick_client {
    uint8_t kickStatus;
    uint16_t reasonLength;
    char reason[];
} msg_kick_client;
#pragma pop

#pragma pack(1)
typedef struct _msg_ping {
    uint32_t playerId;
} msg_ping;
#pragma pop

#pragma pack(1)
typedef struct _msg_list_rooms {
    uint32_t playerId;
} msg_list_rooms;
#pragma pop

#pragma pack(1)
typedef struct _msg_room_announce {
    uint32_t roomId; /* The unique room identifier */
    uint8_t numPlayers; /* Number of players before game starts */
    uint8_t numJoinedPlayers; /* Number of players currently waiting */
    bool passwordProtected; /* True if password protected */
    uint8_t roomNameLen; /* Length of room name string that follows */
    char roomName[]; /* Name of the room */
} msg_room_announce;
#pragma pop

#pragma pack(1)
typedef struct _msg_join_room {
    uint32_t playerId; /* The unique player identifier for basic auth */
    uint32_t roomId; /* The id number of the room the client is joining */
    uint8_t passwordLen; /* The length of the password the user is attempting */
    char password[]; /* The variable length password field */
} msg_join_room;
#pragma pop

#pragma pack(1)
typedef struct _msg_disconnect_client {
    uint32_t playerId;
} msg_disconnect_client;
#pragma pop


void createErrPacket(packet_t *buf, ERR_CODE e);
void reply(packet_t *p, size_t psize, const struct sockaddr *from, int sock);
void setProtocolVers(packet_t *p);

/* This is meant to be used by both client & server */
bool validateLength(packet_t *p, ssize_t len, MSG_TYPE t, \
                    ssize_t *expectedSize);

bool validateName(msg_register_client *m);
bool validateRoomName(msg_create_room *m);

/* These are the base sizes of each type, without the variable components 
 * packed on to the ends */
#define ERRMSG_SIZE sizeof(packet_t)+sizeof(msg_err) 
