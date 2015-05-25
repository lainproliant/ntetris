#include "packet.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "macros.h"
#include <alloca.h>
#include <sys/socket.h>

void createErrPacket(packet_t *ret, ERR_CODE e)
{
    ret->type = (uint8_t)ERR_PACKET;
    *(ret->data) = e;
}

void onsend(uv_udp_send_t *req, int status)
{
    //fprintf(stderr, "in reply callback\n");
    if (status != 0) {
        WARNING("WARNING: %s\n", uv_err_name(status));
    } else {
        fprintf(stderr, "reply callback says status was successful\n");
        free(req);
    }
}

void reply(packet_t *p, size_t psize, uv_udp_t *req, const struct sockaddr *from, int sock)
{
    /*fprintf(stderr, "in reply function\n");
    uv_udp_send_t *send_req = malloc(sizeof(uv_udp_send_t));
    uv_buf_t reply = uv_buf_init((char*)p, psize);
    uv_udp_send(send_req, req, &reply, 1, from, onsend);*/
    if(sendto(sock, p, psize, NULL, from, sizeof(struct sockaddr)) < 0) {
        WARNING("WARNING: something went wrong in the reply - %s\n", strerror(errno));
    }
}

size_t pack_msg_err(msg_err *m, packet_t **buf)
{
    if (*buf == NULL) {
        *buf = malloc(ERRMSG_SIZE);
    }

    (*buf)->type = ERR_PACKET;
    (*buf)->data[0] = m->badMsgCode;
    return ERRMSG_SIZE;
}

size_t pack_msg_register_tetrad(msg_register_tetrad *m, packet_t **buf)
{
    printf("packing register tetrad\n");
    return 0;
}

size_t pack_msg_register_client(msg_register_client *m, packet_t **buf)
{
    size_t packedSize = 2 * sizeof(uint8_t) + m->nameLength;

    if (*buf == NULL) {
        *buf = malloc(packedSize);
    }

    (*buf)->type = (uint8_t)REGISTER_CLIENT;

    /* We can do this because these are all uchars */
    (*buf)->data[0] = m->nameLength;
    memcpy(&((*buf)->data[1]), m->name, m->nameLength);

    return packedSize;
}

size_t pack_msg_update_client(msg_update_client_state *m, packet_t **buf)
{
    size_t packedSize = 3 * sizeof(int) + 2 * sizeof(uint8_t) +\
                        sizeof(uint16_t) * m->nLinesChanged +\
                        sizeof(uint8_t);

    if (*buf == NULL) {
        *buf = malloc(packedSize);
    }

    (*buf)->type = (uint8_t)UPDATE_CLIENT_STATE;
    memcpy((*buf)->data, &htonl(m->nlines), sizeof(int));
    memcpy((*buf)->data + sizeof(int), &htonl(m->score), sizeof(int));
    memcpy((*buf)->data + 2*sizeof(int), &htonl(m->level), sizeof(int));
    (*buf)->data[3*sizeof(int)] = m->status;
    (*buf)->data[3*sizeof(int) + 1] = m->nLinesChanged;

    for (size_t i = 0; i < m->nLinesChanged; ++i) {
        memcpy(&((*buf)->data[3*sizeof(int) + 2]), \
               &htons(m->changedLines[i]), sizeof(uint16_t));
    }

    return packedSize;
}

size_t pack_msg_update_tetrad(msg_update_tetrad *m, packet_t **buf)
{
    size_t packedSize = 5 * sizeof(int);

    if (*buf == NULL) {
        *buf = malloc(packedSize);
    }

    (*buf)->type = (uint8_t)UPDATE_TETRAD;
    memcpy((*buf)->data, &htonl(m->x), sizeof(int));
    memcpy((*buf)->data + sizeof(int), &htonl(m->y), sizeof(int));
    memcpy((*buf)->data + 2 * sizeof(int), &htonl(m->x0), sizeof(int));
    memcpy((*buf)->data + 3 * sizeof(int), &htonl(m->y0), sizeof(int));
    memcpy((*buf)->data + 4 * sizeof(int), &htonl(m->rot), sizeof(int));

    return packedSize;
}
size_t pack_msg_create_room(msg_create_room *m, packet_t **buf)
{
    size_t packedSize = (3 + m->roomNameLen) * sizeof(uint8_t);

    if (*buf == NULL) {
        *buf = malloc(packedSize);
    }

    (*buf)->type = (uint8_t)CREATE_ROOM;
    (*buf)->data[0] = m->numPlayers;
    (*buf)->data[1] = m->roomNameLen;
    memcpy(&((*buf)->data[2]), m->roomName, m->roomNameLen);
    
    return packedSize;
}

size_t pack_user_action(msg_user_action *m, packet_t **buf)
{
    size_t packedSize = 2 * sizeof(uint8_t);

    if (*buf == NULL) {
        *buf = malloc(packedSize);
    }

    (*buf)->type = (uint8_t)USER_ACTION;
    (*buf)->data[0] = m->cmd;

    return packedSize;
}

msg_err unpack_msg_err(packet_t *buf)
{
    msg_err m;
    m.badMsgCode = buf->data[0];

    return m;
}

msg_register_tetrad unpack_register_tetrad(packet_t *buf)
{
    msg_register_tetrad m;

    return m;
}

msg_register_client unpack_msg_register_client(packet_t *buf)
{
    msg_register_client m;
    m.nameLength = buf->data[0];

    /* This may require allocations or copies, not quite sure,
     * test & revisit */
    m.name = (unsigned char[])alloca(m.nameLength);
    memcpy(m.name, &(buf->data[1]), m.nameLength);
    //m.name = (uint8_t[])(&(buf->data[1]));

    return m;
}

msg_update_client_state unpack_msg_update_client(packet_t *buf)
{
    msg_update_client_state m;

    memcpy(&m.nlines, buf->data, sizeof(int)); 
    m.nlines = ntohl(m.nlines);

    memcpy(&m.score, buf->data + sizeof(int), sizeof(int)); 
    m.score = ntohl(m.score);

    memcpy(&m.level, buf->data + 2 * sizeof(int), sizeof(int)); 
    m.level = ntohl(m.level);

    m.status = buf->data[3*sizeof(int)];
    m.nLinesChanged = buf->data[3*sizeof(int) + 1];

    /* Eeep, don't feel great about using alloca function,
     * but don't want to use heap memory either.  While
     * the packet_t buffer is already allocated and will
     * be freed at the end of the work_cb, the elements
     * for it still need to be individually byte swapped
     * for non-network byte ordered architectures */
    m.changedLines = (uint16_t*)alloca(m.nLinesChanged * sizeof(uint16_t));
    size_t ptrOffset = 3*sizeof(int) + 2;

    memcpy(m.changedLines, &buf->data[ptrOffset], \
        m.nLinesChanged * sizeof(uint16_t));

    for (size_t i = 0; i < m.nLinesChanged; ++i) {
        m.changedLines[i] = ntohs(m.changedLines[i]);
    }

    return m;
}

msg_update_tetrad unpack_msg_update_tetrad(packet_t *buf)
{

}

msg_create_room unpack_msg_create_room(packet_t *buf)
{

}

msg_user_action unpack_msg_user_action(packet_t *buf)
{

}
