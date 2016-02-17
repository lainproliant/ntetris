#pragma once

#ifndef __GAME_H
#define __GAME_H

#include "state.h"

typedef enum _PLAYER_ACTION {
    ROTATE_TETRAD, /* Rotate the tetrad in play */
    DROP_TETRAD, /* Drop tetrad to bottom of board */
    INCREMENT_TETRAD, /* Move tetrad an increment */
    MOVE_TETRAD_LEFT, /* Move to the left one increment */
    MOVE_TETRAD_RIGHT, /* Move to the right one increment */
    NUM_ACTIONS
} PLAYER_ACTION;

STATE *initState();
int speedToDelay(int speed);
void destroyState(STATE *s);
int parsePlayerAction(msg_user_action *action, player_t *p);

#endif
