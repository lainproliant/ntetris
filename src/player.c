#include "player.h"
#include "macros.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef __linux__
#include <bsd/string.h>
#else
#include <string.h>
#endif

player_t *createPlayer(const char *name, struct sockaddr sock, unsigned int id)
{
    player_t *p = malloc(sizeof(player_t));
    p->playerId = id;

    /* name param is expected to not be NUL teriminated, this fixes that */
    p->name = name;
    PRINT("Creating player (%u, %s)\n", id, p->name);

    p->state = AWAITING_CLIENT_ACK;
    p->secBeforeNextPingMax = 40;
    p->curJoinedRoomId = 0;
    p->playerAddr = sock;

    return p;
}

void *destroyPlayer(player_t *p)
{
    PRINT("Destroying player object[%p] (%s)\n", p, p->name);
    free(p->name);
    free(p);
}
