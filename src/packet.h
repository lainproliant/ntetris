#pragma once

#include <stdint.h>
#include <uv.h>
#include <stddef.h>

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

/* This makes a bit more sense than a TLV, since L is only a field
 * needed for variable length packets. */
typedef struct _packet_t {
    uint8_t type; /* the type of message */
    uint8_t data[];  /* the raw bytes to marshall and demarshall */
} packet_t;

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

/* Packer functions which build raw byte streams of minimal
 * packed size, if buf == NULL, function performs allocation.
 * this is useful if the user wants to allocate something
 * with a stack variable.  This also performs the neccessary
 * byte swapping for the client/server. Returns back the 
 * size of the allocated byte stream */
size_t pack_msg_err(msg_err *m, packet_t **buf);
size_t pack_msg_register_client(msg_register_client *m, packet_t **buf);
size_t pack_msg_update_client(msg_update_client_state *m, packet_t **buf);
size_t pack_msg_update_tetrad(msg_update_tetrad *m, packet_t **buf);
size_t pack_msg_create_room(msg_create_room *m, packet_t **buf);
size_t pack_user_action(msg_user_action *m, packet_t **buf);

/* Unpacker functions which take raw byte streams pulled
 * from packets and translate them to the correct endianess
 * and padded struct size for the given architecture */
msg_err unpack_msg_err(packet_t *buf);
msg_register_tetrad unpack_register_tetrad(packet_t *buf);
msg_register_client unpack_msg_register_client(packet_t *buf);
msg_update_client_state unpack_msg_update_client(packet_t *buf);
msg_update_tetrad unpack_msg_update_tetrad(packet_t *buf);
msg_create_room unpack_msg_create_room(packet_t *buf);
msg_user_action unpack_msg_user_action(packet_t *buf);

void createErrPacket(packet_t *buf, ERR_CODE e);
void reply(packet_t *p, size_t psize, uv_udp_t *req, const struct sockaddr *from);

/* These are the base sizes of each type, without the variable components 
 * packed on to the ends */
#define ERRMSG_SIZE 2*sizeof(uint8_t)
