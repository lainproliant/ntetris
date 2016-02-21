#include "constants.h"
#include "state.h"
#include "packet.h"
#include "player.h"
#include "room.h"
#include "macros.h"
#include <stdlib.h>
#include <stdio.h>

STATE *initState()
{
    STATE *s = calloc(1, sizeof(STATE));

    /* initialize the game state */
    s->lastUpdate = time(NULL);
    s->init_speed = INIT_SPEED;
    s->speed = INIT_SPEED;

    return s;
}

void destroyState(STATE *s)
{
    if (s->queue != NULL) {
        free(s->queue);
    }

    if (s->field != NULL) {
        free(s->field);
    }
    
    free(s);
}

int parsePlayerAction(msg_user_action *action, player_t *p)
{
    /* Parse the player packet, update the game
     * state accordingly (assuming the game is
     * not in game over status) */
    room_t *room = NULL;
    size_t playerIdx;
    GETRBYID(p->curJoinedRoomId, room);
    STATE *gameState = NULL;

    if (!room || room->state != IN_PROGRESS) {
        goto error;
    }

    for (playerIdx = 0; playerIdx < room->numPlayers; ++playerIdx) {
        if (room->players[playerIdx] == p) {
            break;
        }
    }

    if (playerIdx >= room->numPlayers) {
        WARNING(BOLDRED "player[%u] not found in room's player list\n"
                RESET,  p->playerId);
        goto error;
    }

    readLockRoom(room);
    gameState = room->gameStates[playerIdx];
    /* Parse the actual game logic here, the lock
     * is to ensure the creator of the room cannot destroy
     * it in the middle of parsing the game state and generating
     * a use after free crash */
    readUnlockRoom(room);

    return 0;

error:
    return -1;
}

int speedToDelay(int speed)
{
   int updateDelay = 30 / speed;

   return (updateDelay > 0) ? updateDelay : 1;
}
