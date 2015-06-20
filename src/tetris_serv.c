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

#include <limits.h>
#include <uv.h>
#include <pthread.h>
#include "packet.h"
#include "player.h"
#include "macros.h"

#define DEFAULT_PORT 48879

typedef struct _request {
    ssize_t len;
    void *payload;
    struct sockaddr from;
} request;

static uv_udp_t g_recv_sock;
static uv_fs_t stdin_watcher;

static int vanillaSock;
static char buffer[200];
static uv_buf_t ioVec;

/* This will be the source of randomness, in solaris /dev/urandom
 * is non-blocking but under the hood should use KCF for secure
 * random numbers.  This may not be the case for other platforms,
 * I may toy around with using the entropy values from the random
 * pools provided by these files as a seed to better generators.
 * Honestly this doesn't need to be cryptographically secure, just
 * varied enough and not ridiculously predictable */
static FILE *randFile = NULL;

/* Put rw locks here */
static uv_rwlock_t *playerTableLock = NULL;
static GHashTable *playersByNames = NULL;
static GHashTable *playersById = NULL;

/* This function does necessitate a syscall, so perhaps we should
only call this to a faster PRNG after so many calls */
uint32_t getRand()
{
    uint32_t retVal = 0;
    fread(&retVal, sizeof(uint32_t), 1, randFile);
    return retVal;
}

uint32_t genPlayerId()
{
    /* Find a non-colliding Id, probably will not require many loops */
    uint32_t retVal = getRand();

    do {
        retVal = getRand();
    } while (g_hash_table_lookup(playersById, GINT_TO_POINTER(retVal)));

    return retVal;
}

void kickPlayerByName(const char *name) 
{
    uv_rwlock_wrlock(playerTableLock);
    player_t *p = NULL;
    p = g_hash_table_lookup(playersByNames, name);
    uint8_t kickBufMsg[sizeof(packet_t) + sizeof(msg_kick_client)];
    packet_t *m = kickBufMsg;
    msg_kick_client *mcast = m->data;

    if (p == NULL) {
        WARNING("Player %s not found", name);
    } else {
        // kick packet logic goes here 
        g_hash_table_remove(playersByNames, name);
        g_hash_table_remove(playersById, GINT_TO_POINTER(p->playerId));
        PRINT("Kicked player [%u] (%s)\n", p->playerId, name);

        mcast->reasonLength = 0;
        mcast->kickStatus = KICKED;
        reply(m, sizeof(m), &p->playerAddr, vanillaSock);

        destroyPlayer(p);
    }

    uv_rwlock_wrunlock(playerTableLock);
}

void kickPlayerById(unsigned int id, const char *reason)
{
    uv_rwlock_wrlock(playerTableLock);
    player_t *p = NULL;
    packet_t *m = NULL;
    msg_kick_client *mcast = NULL;
    p = g_hash_table_lookup(playersById, GINT_TO_POINTER(id));

    if (p == NULL) {
        WARNING("Player id: %u not found", id);
    } else {
        g_hash_table_remove(playersByNames, p->name);
        g_hash_table_remove(playersById, GINT_TO_POINTER(p->playerId));
        PRINT("Kicked player [%u] (%s)\n", p->playerId, p->name);

        if (reason != NULL) {
            m = malloc(sizeof(packet_t) + sizeof(msg_kick_client) + 
                        strlen(reason));
            mcast = m->data;
            mcast->reasonLength = htons(strlen(reason));
            memcpy(mcast->reason, reason, strlen(reason));
        } else {
            m = malloc(sizeof(packet_t) + sizeof(msg_kick_client));
            mcast = m->data;
            mcast->reasonLength = 0;
        }

        mcast->kickStatus = KICKED;
        reply(m, sizeof(m), &p->playerAddr, vanillaSock);
        destroyPlayer(p);
        free(m);
    }

    uv_rwlock_wrunlock(playerTableLock);
}

void printPlayer(gpointer k, gpointer v, gpointer d)
{
    player_t *p = (player_t*)v;
    PRINT("\t%s [id=" BOLDRED "%u" 
            RESET", ptr=%p, to=%d]\n", p->name,
                                       p->playerId,
                                       p, p->secBeforeNextPingMax);
}

void printPlayers()
{
    uv_rwlock_rdlock(playerTableLock);
    size_t numPlayers = g_hash_table_size(playersById);

    if (numPlayers == 0) {
        PRINT("[0 players found]\n");
    } else {
        PRINT("PlayerList (%lu players) = \n", numPlayers);
        g_hash_table_foreach(playersById, 
            (GHFunc)printPlayer, NULL);
    }
    
    uv_rwlock_rdunlock(playerTableLock);
}

gboolean gh_subtractSeconds(gpointer k, gpointer v, gpointer d)
{
    packet_t *m = NULL;
    msg_kick_client *mcast = NULL;
    const char *kickMsg = "Stale connection";
    uint8_t kickBufMsg[sizeof(packet_t) + 
                       sizeof(msg_kick_client) +
                       strlen(kickMsg)];
    player_t *p = (player_t*)v;
    p->secBeforeNextPingMax -= GPOINTER_TO_INT(d);

    if (p->secBeforeNextPingMax <= 0) {
        m = (packet_t*)kickBufMsg;
        mcast = m->data;
        mcast->reasonLength = htons(strlen(kickMsg));
        memcpy(mcast->reason, kickMsg, strlen(kickMsg));
        mcast->kickStatus = KICKED;
        reply(m, sizeof(m), &p->playerAddr, vanillaSock);

        /* Remove from the other hashtable before freeing */
        g_hash_table_remove(playersByNames, p->name);

        destroyPlayer(p);

        return TRUE;
    }

    return FALSE;
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
    uv_rwlock_wrlock(playerTableLock);
    g_hash_table_foreach_steal(playersById,
                              (GHRFunc)gh_subtractSeconds, 
                              GINT_TO_POINTER(h->repeat / 1000));
    uv_rwlock_wrunlock(playerTableLock);
}

void gh_freeplayer(gpointer k, gpointer v, gpointer d)
{
    player_t *p = (player_t*)v;
    destroyPlayer(p);
}

void killPlayers(uv_timer_t *h)
{
    uv_rwlock_wrlock(playerTableLock);
    g_hash_table_foreach(playersById, (GHFunc)gh_freeplayer, NULL);
    g_hash_table_remove_all(playersById);
    g_hash_table_remove_all(playersByNames);
    uv_rwlock_wrunlock(playerTableLock);
}

void parse_cmd(const char *cmd)
{
    const char *stn_err_str= NULL;
    const char *pNameOrId = NULL;
    const char *reason = NULL;
    const char *srvcmd = strsep(&cmd, " \n");
    unsigned int id;

    if (!strncmp(srvcmd, "lsplayers", 9)) {
        printPlayers(NULL);
    } else if (!strncmp(srvcmd, "kickname", 8)) {
        pNameOrId = strsep(&cmd, "\n");
        kickPlayerByName(pNameOrId);
    } else if (!strncmp(srvcmd, "kickidreason", 11)) {
        pNameOrId = strsep(&cmd, " ");

        if (pNameOrId == NULL || strlen(pNameOrId) == 0) {
            WARN("Syntax: kickidreason <userid> <reason>");
            return;
        }

        reason = strsep(&cmd, "\n");

        if (reason == NULL || strlen(reason) == 0) {
            WARN("Syntax: kickidreason <userid> <reason>");
            return;
        }

        id = strtonum(pNameOrId, 0, UINT32_MAX, &stn_err_str);

        if (stn_err_str) {
            WARNING("Can't parse uid %s: %s", pNameOrId, stn_err_str);
        } else {
            kickPlayerById(id, reason);
        }
    } else if (!strncmp(srvcmd, "kickall", 6)) {
        killPlayers(NULL);       
    } else if (!strncmp(srvcmd, "kickid", 6)) {
        pNameOrId = strsep(&cmd, "\n");

        if (pNameOrId == NULL || strlen(pNameOrId) == 0) {
            WARN("Syntax: kickid <userid>");
            return;
        }

        id = strtonum(pNameOrId, 0, UINT32_MAX, &stn_err_str);

        if (stn_err_str) {
            WARNING("Can't parse uid %s: %s", pNameOrId, stn_err_str);
        } else {
            kickPlayerById(id, NULL);
        }
    } else {
        WARNING("command %s not recognized", srvcmd);
    }
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

void handle_msg(uv_work_t *req)
{
    /* This function dispatches a request */
    request *r = req->data;
    packet_t *pkt = r->payload;

    /* Create a blank errPkt, populate later with 
     * createErrPacket */
    MSG_TYPE packetType = (MSG_TYPE)pkt->type;
    msg_err errMsg;
    msg_register_client *rclient = NULL;
    msg_create_room *croom = NULL;
    char *clientName = NULL;
    player_t *newPlayer = NULL;
    msg_reg_ack m_ack;
    msg_reg_ack *m_recAck = NULL;
    char senderIP[20] = { 0 };

    /* Stack allocated buffer for the error message packet */
    uint8_t errPktBuf[ERRMSG_SIZE];
    uint8_t ackPackBuf[sizeof(packet_t) + sizeof(msg_reg_ack)];
    packet_t *errPkt = errPktBuf;
    packet_t *ackPack = ackPackBuf;
    ssize_t ePktSize = -1;

    if ( PROTOCOL_VERSION != pkt->version ) {
        WARNING("Incorrect/unexpected protocol version\n"
                "Expected version %d, got %d :(", 
                PROTOCOL_VERSION, pkt->version);
        createErrPacket(errPkt, BAD_PROTOCOL);
        reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
        return;
    }

    /* Validate lengths */
    if(!validateLength((packet_t*)r->payload, r->len, packetType, &ePktSize)) {
        WARNING("Incorrect/unexpected packet length\n"
                "Expected %zd bytes, got %zd bytes", ePktSize, r->len);
        createErrPacket(errPkt, BAD_LEN);
        reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
        return;
    }

    switch(packetType) {
        case REGISTER_TETRAD:
        case ERR_PACKET:
        case KICK_CLIENT:
        case UPDATE_CLIENT_STATE:
            createErrPacket(errPkt, ILLEGAL_MSG);
            reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
            return;
            break;

        case REGISTER_CLIENT:
            rclient = (msg_register_client*)(pkt->data);

            if (!validateName(rclient)) {
                WARN("Non-printable or name length too long!");
                createErrPacket(errPkt, BAD_NAME);
                reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
                return;
            }

            /* This won't be NULL terminated */
            clientName = malloc(rclient->nameLength + 1);
            strlcpy(clientName, rclient->name, rclient->nameLength + 1);

            /* If this far, name meets requirements, now check for collisions */
            uv_rwlock_rdlock(playerTableLock);

            /* If there's no player with that name
             * Note: contains is probably a faster function for this but Oracle
             * and illumos both do not distribute very modern versions of glib
             * which contain these variants */
            if (!g_hash_table_lookup(playersByNames, clientName)) {
                uv_rwlock_rdunlock(playerTableLock);
                
                /* Make sure nothing can read or write to this but us */
                uv_rwlock_wrlock(playerTableLock);

                if (g_hash_table_lookup(playersByNames, clientName)) {
                    WARN("Somebody took our name while waiting for lock");
                    uv_rwlock_wrunlock(playerTableLock);
                    goto name_collide;
                }

                newPlayer = createPlayer(clientName, r->from, genPlayerId());
                g_hash_table_insert(playersByNames, clientName, newPlayer);
                g_hash_table_insert(playersById,
                                    GINT_TO_POINTER(newPlayer->playerId), 
                                    newPlayer);

                uv_rwlock_wrunlock(playerTableLock);

                PRINT("Registering client %s\n", newPlayer->name);
                m_ack.curPlayerId = htonl(newPlayer->playerId);
                ackPack->type = REG_ACK;
                memcpy(ackPack->data, &m_ack, sizeof(msg_reg_ack));
                reply(ackPack, sizeof(ackPackBuf), &r->from, vanillaSock);

            } else { /* Name collision, send back error */
                uv_rwlock_rdunlock(playerTableLock);
name_collide:
                WARNING("Name collision for %s", clientName);
                free(clientName);
                createErrPacket(errPkt, BAD_NAME);
                reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
                return;
            }

            return;
            break;

        case CREATE_ROOM:
            croom = (msg_create_room*)(pkt->data);
            
            if(!validateRoomName(croom)) {
                uv_ip4_name((const struct sockaddr_in*)&r->from, senderIP, 19);
                WARN("Non-printable or too long room name from %s!", senderIP);
                createErrPacket(errPkt, BAD_NAME);
                reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
                return;
            }

            break;

        case REG_ACK:
            m_recAck = (msg_reg_ack*)(pkt->data);
            break;

        default:
            WARN("Unhandled packet type!!!!\n");
            createErrPacket(errPkt, UNSUPPORTED_MSG);
            reply(errPkt, ERRMSG_SIZE, &r->from, vanillaSock);
            return;
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

    char senderIP[20] = { 0 };
    uv_ip4_name((const struct sockaddr_in*)addr, senderIP, 19);
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

    playerTableLock = malloc(sizeof(uv_rwlock_t));
    uv_rwlock_init(playerTableLock);

    /* Hopefully no collisions on playersById */
    playersById = g_hash_table_new(NULL, NULL);
    playersByNames = g_hash_table_new(g_str_hash, g_str_equal);

    uv_loop_t *loop = uv_default_loop();
    uv_udp_init(loop, &g_recv_sock);
    uv_idle_t idler;
    uv_idle_init(loop, &idler);
    uv_idle_start(&idler, idler_task);

    ioVec = uv_buf_init(buffer, 200);
    memset(buffer, 0, sizeof(buffer));
    struct sockaddr_in recaddr;

    uv_fs_read(loop, &stdin_watcher, 0, &ioVec, 1, -1, on_type);

    /* TODO: remove these, they are for testing */
    uv_timer_t timer_req;
    //uv_timer_t timer_req2;

    uv_timer_init(loop, &timer_req);
    //uv_timer_init(loop, &timer_req2);
    uv_timer_start(&timer_req, expireStaleUsers, 15000, 15000);
    //uv_timer_start(&timer_req2, killPlayers, 80000, 80000);
    /* end remove */

    uv_ip4_addr("0.0.0.0", port, &recaddr);
    uv_udp_bind(&g_recv_sock, (const struct sockaddr*)&recaddr, UV_UDP_REUSEADDR);
    uv_udp_recv_start(&g_recv_sock, malloc_cb, onrecv);

    vanillaSock = socket(AF_INET, SOCK_DGRAM, 0);
    uv_run(loop, UV_RUN_DEFAULT);

    /* state cleanup */
    uv_loop_close(uv_default_loop());

    return 0;
}
