#pragma once

#ifndef __STATE_H
#define __STATE_H

#include <glib.h>

/* Forward declarations */
struct _TETRAD;

typedef struct _STATE {
    GQueue* queue;
    char* field;

#ifdef CLIENT
    WINDOW* fieldwin;
    WINDOW* statuswin;
    int keymap[9];

    /* settings */
    int init_level;
    int line_clear_timeout;
    int pause_f;
    char clock[TETRIS_CLOCK_BUFSIZE];
#endif

    time_t lastUpdate;
    unsigned long ticks;
    int line_clear_t;
    int line_clear_f;
    int game_over_f;

    struct _TETRAD* tetrad;

    int status;
    int init_speed;
    int speed;
    int delta;
    int level;
    int lines;
    int score;
    int queue_size;

    int Bx, By; // playfield size vector
    int Sx, Sy; // size of playfield window, in characters
    int Sbx, Sby; // size of status window, in characters
    int Wx, Wy; // size of standard window

    // boolean switches
    int do_clear;
    int do_rotate_timeout_reset;
    int do_dissolve;
    int do_pause_blocks;

} STATE;

#endif
