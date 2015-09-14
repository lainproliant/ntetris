#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <strings.h>
#include <string.h>
#include <glib.h>

#ifdef __sun
#include "strtonum.h"
#include <mtmalloc.h>
#elif defined(__linux__)
#include <bsd/stdlib.h>
#include <bsd/string.h>
#endif

#include <uv.h>
#include <limits.h>
#include <pthread.h>
#include "packet.h"
#include "player.h"
#include "macros.h"
#include "cmd.h"
#include "server.h"

#define DEFAULT_PORT 48879

static uv_fs_t stdin_watcher;

static char buffer[200];
static uv_buf_t ioVec;

/* This will be the source of randomness, in solaris /dev/urandom
 * is non-blocking but under the hood should use KCF for secure
 * random numbers.  This may not be the case for other platforms,
 * I may toy around with using the entropy values from the random
 * pools provided by these files as a seed to better generators.
 * Honestly this doesn't need to be cryptographically secure, just
 * varied enough and not ridiculously predictable */
FILE *randFile = NULL;

/* Put rw locks here */
/*uv_rwlock_t *playerTableLock = NULL;
GHashTable *playersByNames = NULL;
GHashTable *playersById = NULL;*/
server_t *g_server = NULL;

gboolean gh_subtractSeconds(gpointer k, gpointer v, gpointer d)
{
    const char *kickMsg = "Stale connection";
    player_t *p = (player_t*)v;
    p->secBeforeNextPingMax -= GPOINTER_TO_INT(d);

    if (p->secBeforeNextPingMax <= 0) {
        sendKickPacket(p, kickMsg, g_server->listenSock);

        /* Remove from the other hashtable before freeing */
        g_hash_table_remove(g_server->playersByNames, p->name);

        destroyPlayer(p);

        return TRUE;
    }

    return FALSE;
}

void destroy_expire_work_t(uv_work_t *w, int status)
{
    free(w);
}

void expireStaleUsersDispatch(uv_work_t *w)
{
    uv_timer_t *h = (uv_timer_t*)w->data;
    uv_rwlock_wrlock(g_server->playerTableLock);
    g_hash_table_foreach_steal(g_server->playersById,
                              (GHRFunc)gh_subtractSeconds, 
                              GINT_TO_POINTER(h->repeat / 1000));
    uv_rwlock_wrunlock(g_server->playerTableLock);
}

void expireStaleUsers(uv_timer_t *h)
{
    /* Every 15 seconds, look for users that haven't
     * sent a PING message.  Do this by subtracting
     * 15 from the secBeforeNextPingMax.  The client
     * can send a PING every 10 seconds.  This way if
     * we miss a PING message we aren't likely to drop
     * them unfairly.  The steal or remove foreach functions
     * here are the only safe way to do this without messing
     * up the data structure in a foreach.  An iterator would
     * have been another option */

    uv_work_t *workData = (uv_work_t*)malloc(sizeof(uv_work_t));
    workData->data = h;
    uv_queue_work(uv_default_loop(), workData,
                  expireStaleUsersDispatch, destroy_expire_work_t);
}

void on_type(uv_fs_t *req)
{
    /* This is a seemingly backward-ass way to process stdin, but I'm
     * not sure the correct way to do this with libuv callbacks.  There
     * is an example in the documentation that alternatively uses the
     * pipe reader and stream_t for libuv, but the docs say that the
     * program does not always work.  And I'd rather parse stdin directly
     * rather than duplicate things to pipe and have to deal with SIGPIPE.
     * the having to set the callback again is weird, I assume after the
     * callback fires once the descriptor closes for some reason */
    if (req->result > 0) {
        //ioVec.len = req->result;
        parse_cmd(buffer);
        uv_fs_read(uv_default_loop(), &stdin_watcher, 0, &ioVec, 1, -1, on_type);
        memset(buffer, 0, strlen(buffer));
    }
}

void idler_task(uv_idle_t *handle)
{

}

void destroy_msg(uv_work_t *req, int status)
{
    request *r = req->data;
    free(r->payload);
    free(r);
    free(req);

    if (status) {
        WARNING("%s", uv_err_name(status));
    }
}

void onrecv(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf,
            const struct sockaddr *addr, unsigned flags)
{
    if (nread < 0) {
        WARNING("%s", uv_err_name(nread));
        uv_close((uv_handle_t*)req, NULL);
        free(buf->base);
        return;
    }

    if (nread < 2) {
        if(flags & UV_UDP_PARTIAL) {
            WARN("Lost some of the buffer");
        }
        free(buf->base);
        return;
    }

    char senderIP[17] = { 0 };
    uv_ip4_name((const struct sockaddr_in*)addr, senderIP, 16);
    in_port_t port = ((const struct sockaddr_in*)addr)->sin_port;

    uv_work_t *work = (uv_work_t*)malloc(sizeof(uv_work_t));

    request *theReq = malloc(sizeof(request));
    theReq->payload = buf->base;
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
    const char *stn_err_str = NULL;

    static struct option longopts[] = {
        {"port",      required_argument,     NULL,     'p'},
        {"random",      required_argument,     NULL,     'r'},
        {NULL,        0,                     NULL,     0}
    };

    while ((go_ret = getopt_long(argc, argv, "p:r:", longopts, NULL)) != -1) {
       switch (go_ret) {
            case 'p':
                port = strtonum(optarg, 1, UINT16_MAX, &stn_err_str);
                if (stn_err_str) {
                    ERROR("Bad value for port: %s", stn_err_str);
                }
            case 'r':
                randFile = fopen(optarg, "r");
       }
    }

    if (!randFile) {
        randFile = fopen("/dev/urandom", "r");
    }

    /* There were many possible approaches to take to deal with the lack
     * of thread safety in libuv's uv_udp_send() functions.  These include
     * async_send, multiple outbound sockets on a per thread basis,
     * constructing a raw socket based send call, or constructing a send
     * queue of messages when the worker thread has completed.  For now
     * we'll try the raw socket approach */

    g_server = initializeServer("0.0.0.0", port);
    uv_idle_start(&g_server->idler, idler_task);

    ioVec = uv_buf_init(buffer, 200);
    memset(buffer, 0, sizeof(buffer));

    uv_fs_read(g_server->mainLoop, &stdin_watcher, 0, &ioVec, 1, -1, on_type);
    uv_timer_start(&g_server->timer_req, expireStaleUsers, 15000, 15000);

    uv_udp_recv_start(&g_server->recv_sock, malloc_cb, onrecv);

    uv_run(g_server->mainLoop, UV_RUN_DEFAULT);

    /* state cleanup */
    uv_loop_close(g_server->mainLoop);

    return 0;
}
