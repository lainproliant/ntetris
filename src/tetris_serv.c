#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <strings.h>

#ifdef __sun
#include "strtonum.h"
#include <mtmalloc.h>
#else
#include <strtonum.h>
#endif

#include <limits.h>
#include <uv.h>
#include <pthread.h>
#include "packet.h"
#include "macros.h"

#define DEFAULT_PORT 48879

uv_udp_t g_recv_sock;
uv_udp_t g_send_sock;

static int vanillaSock;

typedef struct _request {
    ssize_t len;
    void *payload;
    struct sockaddr from;
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

    /* Validate lengths */
    if(!validateLength((packet_t*)r->payload, r->len, packetType)) {
        WARN("Incorrect/unexpected packet length\n");
        createErrPacket(errPkt, BAD_LEN);
        reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
        return;
    }

    switch(packetType) {
        case REGISTER_TETRAD:
            createErrPacket(errPkt, ILLEGAL_MSG);
            reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
            break;

        case REGISTER_CLIENT:
            createErrPacket(errPkt, SUCCESS);
            reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
            break;

        default:
            WARN("Unhandled packet type!!!!\n");
            createErrPacket(errPkt, UNSUPPORTED_MSG);
            reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
            break;
    }
}

void destroy_msg(uv_work_t *req, int status)
{
    request *r = req->data;
    free(r->payload);
    free(r);
    free(req);

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
    in_port_t port = ((const struct sockaddr_in*)addr)->sin_port;
    //fprintf(stderr, "Received msg over %s:%d\n", senderIP, port);

    uv_work_t *work = (uv_work_t*)malloc(sizeof(uv_work_t));

    request *theReq = malloc(sizeof(request));
    theReq->payload = buf->base;
    //memcpy(theReq->payload, buf->base, nread);
    //free(buf->base);
    theReq->len = nread;
    theReq->from = *addr;

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

    /* There were many possible approaches to take to deal with the lack
     * of thread safety in libuv's uv_udp_send() functions.  These include
     * async_send, multiple outbound sockets on a per thread basis,
     * constructing a raw socket based send call, or constructing a send
     * queue of messages when the worker thread has completed.  For now
     * we'll try the raw socket approach */

    uv_loop_t *loop = uv_default_loop();
    uv_udp_init(loop, &g_recv_sock);
    //uv_udp_init(loop, &g_send_sock);
    struct sockaddr_in recaddr;

    uv_ip4_addr("0.0.0.0", port, &recaddr);
    uv_udp_bind(&g_recv_sock, (const struct sockaddr*)&recaddr, UV_UDP_REUSEADDR);
    uv_udp_recv_start(&g_recv_sock, malloc_cb, onrecv);

    vanillaSock = socket(AF_INET, SOCK_DGRAM, 0);

    return uv_run(loop, UV_RUN_DEFAULT);
}
