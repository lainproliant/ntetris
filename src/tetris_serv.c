#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#ifdef __sun
#include "strtonum.h"
#include <mtmalloc.h>
#else
#include <strtonum.h>
#endif

#include <limits.h>
#include <uv.h>
#include "packet.h"
#include "macros.h"

#define DEFAULT_PORT 48879

uv_udp_t g_recv_sock;

typedef struct _request {
    size_t len;
    void *payload;
    const struct sockaddr *from;
} request;

void handle_msg(uv_work_t *req)
{
    /* This function dispatches a request */
    request *r = req->data;
    uint8_t rawPacketType = ((uint8_t*)(r->payload))[0];

    /* Create a blank errPkt, populate later with 
     * createErrPacket */
    MSG_TYPE packetType = (MSG_TYPE)rawPacketType;
    msg_err errMsg;

    /* Stack allocated buffer for the error message packet */
    uint8_t errPktBuf[ERRMSG_SIZE];
    packet_t *errPkt = &errPktBuf;

    size_t errbytes = pack_msg_err(&errMsg, &errPkt);

    switch(packetType) {
        case REGISTER_TETRAD:
            printf("Handling REGISTER_TETRAD\n");
            createErrPacket(errPkt, UNSUPPORTED_MSG);
            reply(errPkt, errbytes, &g_recv_sock, r->from);
            free(errPkt);
            break;
        default:
            WARN("Unhandled packet type!!!!\n");
            createErrPacket(errPkt, UNSUPPORTED_MSG);
            reply(errPkt, errbytes, &g_recv_sock, r->from);
            break;
    }
}

void destroy_msg(uv_work_t *req, int status)
{
    request *r = req->data;
    free(r->payload);
    free(r);

    if(status) {
        WARNING("WARNING: %s\n", uv_err_name(status));
    }
}

void onrecv(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf,
            const struct sockaddr *addr, unsigned flags)
{
    if (nread < 0) {
        WARNING("WARNING: %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*)req, NULL);
        free(buf->base);
        return;
    }

    if (nread == 0) {
        if(flags & UV_UDP_PARTIAL) {
            WARN("Lost some of the buffer\n");
        }
        free(buf->base);
        return;
    }

    char senderIP[20] = { 0 };
    uv_ip4_name((const struct sockaddr_in*)addr, senderIP, 19);

    uv_work_t *work = (uv_work_t*)malloc(sizeof(uv_work_t));

    request *theReq = (request*)malloc(sizeof(request));
    theReq->payload = buf->base;
    theReq->len = nread;
    theReq->from = addr;

    work->data = theReq;
    uv_queue_work(uv_default_loop(), work, handle_msg, destroy_msg);
}

static void malloc_cb(uv_handle_t *h, size_t s, uv_buf_t *b)
{
    b->base = (char*)malloc(s * sizeof(char));
    assert(b->base != NULL);
    b->len = s;
}

int main(int argc, char *argv[])
{
    int go_ret;
    int port = DEFAULT_PORT;
    const char *err_str = NULL;

    static struct option longopts[] = {
        {"port",      required_argument,     NULL,     'p'},
        {NULL,        0,                     NULL,     0}
    };

    while ((go_ret = getopt_long(argc, argv, "p:", longopts, NULL)) != -1) {
       switch (go_ret) {
            case 'p':
                port = strtonum(optarg, 1, UINT16_MAX, &err_str);
                if (err_str) {
                    ERR("Bad value for port");
                }
       }
    }

    uv_loop_t *loop = uv_default_loop();
    uv_udp_init(loop, &g_recv_sock); 
    struct sockaddr_in recaddr;

    uv_ip4_addr("0.0.0.0", port, &recaddr);
    uv_udp_bind(&g_recv_sock, (const struct sockaddr*)&recaddr, UV_UDP_REUSEADDR);
    uv_udp_recv_start(&g_recv_sock, malloc_cb, onrecv);

    return uv_run(loop, UV_RUN_DEFAULT);
}
