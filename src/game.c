#include "constants.h"
#include "state.h"
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

int speedToDelay(int speed)
{
   int updateDelay = 30 / speed;

   return (updateDelay > 0) ? updateDelay : 1;
}
