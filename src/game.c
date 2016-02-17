#include "constants.h"
#include "state.h"
#include "packet.h"
#include "player.h"
#include <stdlib.h>

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

int parsePlayerAction(msg_user_action *action)
{
    /* Parse the player packet, update the game
     * state accordingly (assuming the game is
     * not in game over status) */

     return 0;
}

int speedToDelay(int speed)
{
   int updateDelay = 30 / speed;

   return (updateDelay > 0) ? updateDelay : 1;
}
