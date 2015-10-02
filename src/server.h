#pragma once
#ifndef __SERVER_H
#define __SERVER_H

#include <glib.h>
#include <uv.h>

/* Struct that stores the global
 * server state objects */
typedef struct _server_t {
   GHashTable *playersByNames; /* protected by playerTableLock */
   GHashTable *playersById; /* protected by playerTableLock */
   GHashTable *roomsById; /* protected by roomsLock */
   GHashTable *roomsByName; /* protected by roomsLock */
   uv_rwlock_t *playerTableLock; /* player lists rw_lock */
   uv_rwlock_t *roomsLock; /* Protects the global list of rooms */
   uv_loop_t *mainLoop;
   int listenSock;
   uv_timer_t timer_req;
   uv_udp_t recv_sock;
   uv_idle_t idler;
} server_t;

server_t *initializeServer(const char *ipaddr, int port);

#endif
