#include "player.h"
#include "macros.h"
#include <stdlib.h>

#ifdef __linux__
#include <bsd/string.h>
#else
#include <string.h>
#endif

player_t *createPlayer(const char *name, uint16_t nameLen, 
                       struct sockaddr sock, unsigned int id)
{
    player_t *p = malloc(sizeof(player_t));
    p->playerId = id;

    /* name param is expected to not be NUL teriminated, this fixes that */
    p->name = malloc(nameLen + 1);
    strlcpy(p->name, name, nameLen + 1);
    PRINT("Creating player (%u, %s)\n", id, p->name);

    p->state = AWAITING_CLIENT_ACK;
    p->secBeforeNextPingMax = 20;
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
