#include "server.h"
#include <stdlib.h>

server_t *initializeServer(const char *ipaddr, int port)
{
    struct sockaddr_in recaddr;
    server_t *retVal = calloc(1, sizeof(server_t));
    retVal->mainLoop = uv_default_loop();
    
    /* Hopefully no collisions on playersById */
    retVal->playersById = g_hash_table_new(NULL, NULL);
    retVal->playersByNames = g_hash_table_new(g_str_hash, g_str_equal);

    retVal->roomsById = g_hash_table_new(NULL, NULL);
    retVal->roomsByName = g_hash_table_new(g_str_hash, g_str_equal);

    /* initialize rwlocks */
    retVal->playerTableLock = malloc(sizeof(uv_rwlock_t));
    retVal->roomsLock = malloc(sizeof(uv_rwlock_t));

    uv_rwlock_init(retVal->playerTableLock);
    uv_rwlock_init(retVal->roomsLock);
    uv_timer_init(retVal->mainLoop, &retVal->timer_req);
    uv_idle_init(retVal->mainLoop, &retVal->idler);
    uv_udp_init(retVal->mainLoop, &retVal->recv_sock);

    uv_ip4_addr(ipaddr, port, &recaddr);
    uv_udp_bind(&retVal->recv_sock, (const struct sockaddr*)&recaddr, 
                UV_UDP_REUSEADDR);

    /* "Bind" to this so outbound is the same the socket was received over */
    retVal->listenSock = retVal->recv_sock.io_watcher.fd;

    return retVal;
}
