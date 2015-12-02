/*
 * ntetris: a tetris clone
 * (c) 2008 Lee Supe (lain_proliant)
 * Released under the GNU General Public License
 */
#define CLIENT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>
#include <ncurses.h>
#include <glib.h>
#include "tetris.h"
#include <limits.h>

#ifdef __linux__
#include <bsd/stdlib.h>
#include <bsd/string.h>
#endif

#ifdef __sun
#include "strtonum.h"
#endif

#define TETRIS_DEBUG

int main(int argc, char* argv[])
{
    STATE* state;

    state = Init(argc, argv);
    if (!state) {
        fprintf(stderr, "<ntetris>\tCould not initialize state.  Abort.\n");
        return 0;
    }

    while(state->status != STATUS_GAMEOVER) {
        state->ticks ++;

        Input(state);
        Update(state);
        Paint(state);
        Refresh(state);

        napms(REFRESH_DELAY);
    }

    Cleanup(state);

    return 0;
}

int ParseOptions(STATE *state, int argc, char *argv[])
{
    int go_ret, width, height, delay, level;
    const char *err_str;
    char *next_key = NULL;
    char *next_key_val = NULL;

    static struct option longopts[] = {
         { "clear",      required_argument,  NULL, 'c' },
         { "level",      required_argument,  NULL, 'L' },
         { "width",      required_argument,  NULL, 'x' },
         { "height",     required_argument,  NULL, 'y' },
         { "delay",      required_argument,  NULL, 'd' },
         { "pause-show", no_argument,        NULL, 'p' },
         { "keys",       required_argument,  NULL, 'k' },
         { NULL,         0,                  NULL, 0 }
    };


    while ((go_ret = getopt_long(argc, argv, "c:L:x:y:d:k:p", longopts, NULL)) != -1) {
        switch (go_ret) {
            case 'c':
                if (! strcmp(optarg, "none")) {
                    state->do_clear = CLEAR_NONE;
                } else if (! strcmp(optarg, "flash")) {
                    state->do_clear = CLEAR_FLASH;
                } else if (! strcmp(optarg, "blank")) {
                    state->do_clear = CLEAR_BLANK;
                } else {
                    fprintf(stderr, "<ntetris>\tInvalid line clear mode: \"%s\"\n",
                            optarg);
                    return 0;
                }
                break;
            case 'L':
                fprintf(stderr,"%s\n", optarg);
                level = strtonum(optarg, 1, 1000, &err_str);
                if (err_str) {
                    fprintf(stderr, "error parsing level field: %s\n", err_str);
                    return 0;
                }

                state->init_level = level;
                break;
            case 'x':
                /* Let's use the bsd safe strtonum functions for this */
                width = strtonum(optarg, 10, 1000, &err_str);
                if (err_str) {
                    fprintf(stderr, "error parsing width field: %s\n", err_str);
                    return 0;
                }

                state->Bx = width;
                break;
            case 'y':
                height = strtonum(optarg, 10, 1000, &err_str);
                if (err_str) {
                    fprintf(stderr, "error parsing height field: %s\n", err_str);
                    return 0;
                }

                state->By = height;
                break;
            case 'd':
                delay = strtonum(optarg, 0, 10, &err_str);
                if (err_str) {
                    fprintf(stderr, "error parsing delay field: %s\n", err_str);
                    return 0;
                }

                state->line_clear_timeout = delay;
                break;
            case 'p':
                state->do_pause_blocks = !state->do_pause_blocks;
                break;
            case 'k':
                    while ((next_key = strsep(&optarg, " ")) != NULL) {

                        size_t A;
                        next_key_val = strsep(&optarg, " ");

                        for (A = 0; A < TETRIS_KEYS; A++) {
                            if (! strcmp(keymap_desc[A], next_key)) {
                                state->keymap[A] = KeyParse(next_key_val);
                                break;
                            }
                        }

                        if (A >= TETRIS_KEYS) {
                            fprintf(stderr, "<ntetris>\tAction not supported: \"%s\".\n",
                                    next_key);
                            return 0;
                        }
                }
                break;
            default:
                return 0;
                break;
        }

    }

    return 1;

}

STATE* Init(int argc, char* argv[])
{
    STATE* state;

    state = (STATE*)malloc(sizeof(STATE));
    if (!state)
        return NULL;

    memset(state, 0, sizeof(STATE));
    srand(time(0));

    state->Bx = 10;
    state->By = 20;
    state->queue = g_queue_new();
    state->fieldwin = NULL;
    state->queue_size = 5;
    state->init_speed = INIT_SPEED;
    state->delta = DELTA_SPEED;

    state->init_level = 1;
    state->line_clear_timeout = 0;

    state->do_clear = CLEAR_FLASH;
    state->do_rotate_timeout_reset = 0;
    state->do_dissolve = 1;
    state->do_pause_blocks = 0;

    // setup the default keymap
    state->keymap[TETRIS_KEY_QUIT]              = KeyParse("q");
    state->keymap[TETRIS_KEY_DROP]              = KeyParse("d");
    state->keymap[TETRIS_KEY_LOWER]             = KeyParse("s");
    state->keymap[TETRIS_KEY_ROTATE_CW]         = KeyParse("k");
    state->keymap[TETRIS_KEY_ROTATE_CCW]        = KeyParse("e");
    state->keymap[TETRIS_KEY_MOVE_LEFT]         = KeyParse("j");
    state->keymap[TETRIS_KEY_MOVE_RIGHT]        = KeyParse("l");
    state->keymap[TETRIS_KEY_PAUSE]             = KeyParse("p");
    state->keymap[TETRIS_KEY_RESET]             = KeyParse("r");

    if (! ParseOptions(state, argc, argv)) {
        Cleanup(state);
        return NULL;
    }

    // allocate the field data
    state->field = (char*)malloc(state->Bx * state->By);
    // game instance specific initiailzation
    Reset(state);
    // initialize the curses session
    InitTerminal(state);
    // orient the windows
    DimensionWindows(state);

    // TODO: we need to catch SIGWINCH,
    //       to be able to resize accordingly.
    // signal(SIGWINCH, SignalHandler);

    return state;
}

void InitTerminal(STATE* state)
{
    size_t X = 0;

    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        use_default_colors();
    }

    for(X = 1; X <= 8; X++) {
        init_pair(X, X, X);
        init_pair(X + 20, X, 0);
    }

    init_pair(11, COLOR_WHITE, COLOR_BLACK);
    init_pair(12, COLOR_YELLOW, COLOR_BLACK);
    init_pair(13, COLOR_BLACK, COLOR_WHITE);
    init_pair(14, COLOR_GREEN, COLOR_BLACK);
    init_pair(15, COLOR_CYAN, COLOR_BLACK);
    return;
}

void DimensionWindows(STATE* state)
{
    if (state->fieldwin) {
        delwin(state->fieldwin);
        erase();
    }

    if (state->statuswin) {
        delwin(state->statuswin);
        erase();
    }

    getmaxyx(stdscr, state->Wy, state->Wx);
    state->Sy = state->By + 2;
    state->Sx = 2 * state->Bx + 2;
    state->fieldwin = newwin(state->Sy,
            state->Sx,
            (state->Wy - state->Sy) / 2,
            (state->Wx - state->Sx) / 2);

    state->Sby = TETRIS_STATUS_HEIGHT;
    state->Sbx = TETRIS_STATUS_WIDTH;

    if (state->Sbx) {
        state->statuswin = newwin(state->Sby,
                state->Sbx,
                (state->Wy - state->Sby) / 2,
                (state->Wy - state->Sx) + 1);
    }

    //wbkgd(state->fieldwin, COLOR_PAIR(11));

    return;
}

void Reset(STATE* state)
{
    TETRAD* tetrad;
    size_t X = 0;

    state->status = STATUS_GAME;

    state->ticks = 0;
    state->speed = state->init_speed;
    state->level = state->init_level;
    state->lines = 0;
    state->score = 0;
    state->line_clear_t = 0;
    state->line_clear_f = 0;
    state->game_over_f = 0;
    state->pause_f = 0;

    for(X = 0; X < state->Bx * state->By; X++) {
        state->field[X] = 0;
    }

    while((tetrad = g_queue_pop_tail(state->queue)))
        TetradFree(tetrad);

    // push several new tetrads onto the stack
    while (g_queue_get_length(state->queue) < state->queue_size + 1)
        TetradQueue(state);

    state->tetrad = g_queue_pop_tail(state->queue);

    return;
}

void Cleanup(STATE* state)
{
    endwin();
    free(state->field);
    if (state->tetrad)
        TetradFree(state->tetrad);
    free(state);

    return;
}

void Input(STATE* state)
{
    int c = 0;

    c = getch();
    if (c == ERR)
        return;


    if (c == state->keymap[TETRIS_KEY_QUIT]) {
        state->status = STATUS_GAMEOVER;
    } else if (c == state->keymap[TETRIS_KEY_DROP]) {
        EventDrop(state);
    } else if (c == state->keymap[TETRIS_KEY_LOWER]) {
        EventLower(state);
    } else if (c == state->keymap[TETRIS_KEY_ROTATE_CW]) {
        EventRotate(state, 1);
    } else if (c == state->keymap[TETRIS_KEY_ROTATE_CCW]) {
        EventRotate(state, -1);
    } else if (c == state->keymap[TETRIS_KEY_MOVE_LEFT]) {
        EventMove(state, -1);
    } else if (c == state->keymap[TETRIS_KEY_MOVE_RIGHT]) {
        EventMove(state, 1);
    } else if (c == state->keymap[TETRIS_KEY_PAUSE]) {
        if (! state->game_over_f) {
            if (state->pause_f)
                EventUnpause(state);
            else
                EventPause(state);
        }
    } else if (c == state->keymap[TETRIS_KEY_RESET]) {
        Reset(state);
    } else {
        printw("\a");
    }

    flushinp();
    return;
}

void Paint(STATE* state)
{
    size_t X = 0, Y = 0;

    attron(COLOR_PAIR(12) | A_BOLD);
    mvprintw(0, 0, "%s\n", gs_appname);

    move(state->Wy - 1, 0);
    for (X = 0; X < sizeof(state->keymap) / sizeof(int); X++) {
        attron(COLOR_PAIR(15));
        printw("%s", keymap_desc[X]);
        attron(COLOR_PAIR(11));
        printw("[");
        attron(COLOR_PAIR(14));
        printw("%s", keyname(state->keymap[X]));
        attron(COLOR_PAIR(11));
        printw("] ");
    }

    attroff(A_BOLD);
    attron(COLOR_PAIR(13));
    mvprintw(state->Wy - 1, state->Wx - strlen(state->clock),
            "%s", state->clock);

    // begin painting the status window
    if (state->statuswin) {
        StatusWindowPaint(state);
    }

    // begin painting the board
    werase(state->fieldwin);
    wattrset(state->fieldwin, A_NORMAL);
    box(state->fieldwin, 0, 0);

    if (! state->pause_f || state->do_pause_blocks) {
        for (Y = 0; Y < state->By; Y++) {
            wmove(state->fieldwin, Y + 1, 1);
            for (X = 0; X < state->Bx; X++) {
                wattrset(state->fieldwin, A_NORMAL);
                if (state->field[state->Bx * Y + X] == CLEARED) {
                        switch (state->do_clear) {
                        case CLEAR_FLASH:
                            wattron(state->fieldwin, COLOR_PAIR(rand() % 7 + 1));
                            break;
                        case CLEAR_BLANK:
                        default:
                            break;
                    }
                } else {
                    wattron(state->fieldwin, COLOR_PAIR(state->field[state->Bx * Y + X]));
                }

                waddch(state->fieldwin, ' ');
                waddch(state->fieldwin, ' ');
            }
        }
    }

    if (state->pause_f) {
        StatusMessage(state, state->fieldwin, gs_pause);
    }

    wattrset(state->fieldwin, A_NORMAL);
    if (state->game_over_f) {
        StatusMessage(state, state->fieldwin, gs_gameover);
    }


    // paint the tetrad
    if (state->tetrad)
        TetradPaint(state->fieldwin,
                state->tetrad->y + 1,
                state->tetrad->x + 1,
                state->tetrad);

    return;
}

void Update(STATE* state)
{
    time_t rawtime;
    struct tm* timeinfo;

    // update the clock
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(state->clock, TETRIS_CLOCK_BUFSIZE, "[%H.%M:%S]", timeinfo);

    state->level = state->lines / 10 + state->init_level;
    state->speed = state->init_speed - (state->delta * state->level);

    if (state->game_over_f || state->pause_f)
        return;

    if (state->line_clear_f) {
        if (state->line_clear_t >=
                (state->line_clear_timeout ? state->line_clear_timeout : state->speed)
                || ! state->do_clear) {

            state->line_clear_f = 0;
            LineClear(state);
            EventQuery(state);

        } else if (! state->pause_f) {
            state->line_clear_t ++;
        }
    }

    if (state->tetrad && !TetradUpdate(state)) {
        EventTetrad(state);
    }

    return;
}

void Refresh(STATE* state)
{
    refresh();
    wrefresh(state->fieldwin);
    if (state->statuswin)
        wrefresh(state->statuswin);

    return;
}

void StatusWindowPaint(STATE* state)
{
    size_t Y;
    gint X = 0;

    werase(state->statuswin);
    wattrset(state->statuswin, A_NORMAL);
    box(state->statuswin, 0, 0);

    wmove(state->statuswin, 1, 1);
    wattron(state->statuswin, A_BOLD);
    wprintw(state->statuswin, "Level:\t");
    wattrset(state->statuswin, COLOR_PAIR(20 + (state->level % 7) + 1) | A_BOLD);
    wprintw(state->statuswin, "%d", state->level);

    wmove(state->statuswin, 2, 1);
    wattrset(state->statuswin, A_BOLD);
    wprintw(state->statuswin, "Lines:\t");
    wattrset(state->statuswin, COLOR_PAIR(20 + ((state->lines / 10) % 7) + 1) | A_BOLD);
    wprintw(state->statuswin, "%d", state->lines);

    wmove(state->statuswin, 3, 1);
    wattrset(state->statuswin, A_BOLD);
    wprintw(state->statuswin, "Score:\t");
    wattrset(state->statuswin, COLOR_PAIR(20 + (state->score / 10000 % 7) + 1) | A_BOLD);
    wprintw(state->statuswin, "%d", state->score);

    wmove(state->statuswin, 4, 1);
    wattrset(state->statuswin, A_BOLD);
    wprintw(state->statuswin, "Next:");

    Y = 6;
    for (X = g_queue_get_length(state->queue) - 1; X > 0; X--) {
       TetradPaint(state->statuswin, Y, 1, (TETRAD*)g_queue_peek_nth(state->queue, X));
       Y += 3;
    }

    return;
}

int SignalHandler(int sig)
{
    // TODO: Handle SIGWINCH, maybe SIGSEGV
    // NOTE: signal(sigwinch, SignalHandler);
    return 1;
}

TETRAD* TetradAlloc(int shape, int x, int y)
{
    TETRAD* tetrad = NULL;

    tetrad = (TETRAD*)malloc(sizeof(TETRAD));
    if (!tetrad)
        return NULL;

    tetrad->shape = shape;
    tetrad->x = x;
    tetrad->y = y;
    tetrad->x0 = x;
    tetrad->y0 = y;
    tetrad->t = 0;
    tetrad->rot = 0;

    return tetrad;
}

TETRAD* TetradRandomAlloc(STATE* state)
{
    TETRAD* tetrad;
    int rot = 0;

    rot = rand() % 4;
    tetrad = TetradAlloc(rand() % 7, rand() % state->Bx, 0);

    return tetrad;
}

void TetradFree(TETRAD* tetrad)
{
    free(tetrad);

    return;
}

int TetradUpdate(STATE* state)
{
    if (! state->tetrad) {
        // there is no tetrad
        return 0;
    }

    if (state->tetrad->t >= state->speed) {
        // lower the tetrad
        state->tetrad->y ++;
        if (TetradFieldOverlap(state)) {
            // replace the tetrad
            state->tetrad->y --;

            return 0;
        }

        state->tetrad->t = 0;
    }

    state->tetrad->t ++;
    return 1;
}

void TetradQueue(STATE* state)
{
    TETRAD* tetrad = NULL;

    tetrad = TetradAlloc(rand() % 7,
            state->Bx / 2 - 2,
            0);
    g_queue_push_head(state->queue, tetrad);

    return;
}

void TetradPaint(WINDOW* window, int y, int x, TETRAD* tetrad)
{
    size_t X = 0;
    int Z = 0;
    int W = 0;

    Z = tetrad->rot % 2 ? 2 : 4;
    W = RotateCorrection(tetrad);

    for(X = 8 * tetrad->rot; X < 8 * tetrad->rot + 8; X ++) {
        wmove(window, y + X / Z - W, 2 * (x + X % Z) - 1);
        wattrset(window, A_NORMAL);
        if (shapes[tetrad->shape][X] == '#') {
            wattron(window, COLOR_PAIR(tetrad->shape + 1));
            waddch(window, ' ');
            waddch(window, ' ');
        }
    }

    return;
}

void TetradTranslate(STATE* state, TETRAD* tetrad)
{
    size_t X = 0, offset = 0;
    int Z = 0;
    int W = 0;

    Z = tetrad->rot % 2 ? 2 : 4;
    W = RotateCorrection(tetrad);

    for(X = 8 * tetrad->rot; X < 8 * tetrad->rot + 8; X ++) {
        offset = state->Bx *
            (tetrad->y + X / Z - W) +
            (tetrad->x + X % Z);

        if (shapes[tetrad->shape][X] == '#' &&
                (offset / state->Bx < state->By &&
                 tetrad->x + X % Z < state->Bx)) {
            state->field[offset] = tetrad->shape + 1;
        }
    }

    return;
}

int TetradFieldOverlap(STATE* state)
{
    size_t X = 0, offset = 0;
    int Z = 0;
    int W = 0;

    // calculate the orientation divisor of the tetrad
    Z = state->tetrad->rot % 2 ? 2 : 4;
    W = RotateCorrection(state->tetrad);

    // check for collision (overlapping)
    for(X = 8 * state->tetrad->rot; X < 8 * state->tetrad->rot + 8; X ++) {
        offset = state->Bx *
            (state->tetrad->y + X / Z - W) +
            (state->tetrad->x + X % Z);

        if (shapes[state->tetrad->shape][X] == '#' &&
                ((offset / state->Bx >= state->By
                  || state->tetrad->x + X % Z >= state->Bx) ||
                 (state->field[offset]))) {
            // collision has occured
            return 1;
        }
    }

    return 0;
}

int TetradDrop(STATE* state)
{
    int n = 0;

    if (! state->tetrad)
        return 0;

    for(n = 0; ! TetradFieldOverlap(state); n ++)
        state->tetrad->y ++;

    state->tetrad->y --;

    return n;
}

int RotateCorrection(TETRAD* tetrad)
{
    // NOTE: HEY! Don't fix it if its not broken! >_<
    // TODO: Maybe its not broken, but the math is
    // wrong somewhere.  Fix this!
    int W = 0;

    switch(tetrad->rot) {
        case 0:
            W = 0;
            break;
        case 1:
        case 2:
            W = 4;
            break;
        case 3:
            W = 12;
            break;
        default:
            // ???
            return 0;
    }

    return W;
}

int LineMark(STATE* state, int y, int h)
{
    size_t Y = 0, X = 0;
    int b = 0;
    int n = 0;

    for (Y = y; Y < y + h; Y ++) {
        b = 1;
        for (X = 0; X < state->Bx; X ++) {
            if (! state->field[Y * state->Bx + X]) {
                b = 0;
                break;
            }
        }

        if (b) {
            for (X = 0; X < state->Bx; X ++) {
                state->field[Y * state->Bx + X] = CLEARED;
            }
            n ++;
        }
    }

    return n;
}

void LineClear(STATE* state)
{
    size_t Y = 0;
    int n = 0, x = 0;

    for (Y = 0; Y < state->By; Y += n) {
        n = 1;
        if (state->field[Y * state->Bx] == CLEARED) {
            // how many consecutive lines are cleared?
            for (n = 1; state->field[(Y + n) * state->Bx] == CLEARED; n ++);
            // move all above lines down by n lines
            for (x = Y * state->Bx - 1; x >= 0; x --) {
                state->field[x + n * state->Bx] = state->field[x];
                state->field[x] = 0;
            }
            state->lines += n;
            state->score += Power(2, n - 1) * 1000;
        }
    }

    return;
}

// some useless function
/*
   int PrintTetrad(FILE* file, tetrad_t tetrad, int rot)
   {
   int n = 0;
   size_t X = 0, Y = 0;

   if (rot % 2) {
   for(X = 8 * rot; X < 8 * rot + 8; X += 2) {
   fprintf(file, "%.2s\n", shapes[tetrad] + X);
   }
   } else {
   for(X = 8 * rot; X < 8 * rot + 8; X += 4) {
   fprintf(file, "%.4s\n", shapes[tetrad] + X);
   }
   }

   fputc('\n', file);
   ++ n;

   return n;
   }
   */

void EventQuit(STATE* state)
{
    state->status = STATUS_GAMEOVER;
    return;
}

void EventDrop(STATE* state)
{
    if (! state->tetrad || state->pause_f || state->game_over_f)
        return;

    state->score += TetradDrop(state) * 10;
    EventTetrad(state);

    return;
}

void EventLower(STATE* state)
{
    if (! state->tetrad || state->pause_f)
        return;

    state->tetrad->y ++;
    if (TetradFieldOverlap(state)) {
        state->tetrad->y --;
    }

    return;
}

void EventRotate(STATE* state, int rot)
{
    if (! state->tetrad || state->pause_f)
        return;

    state->tetrad->rot = (state->tetrad->rot + rot + 4) % 4;
    // TODO: implement smart rotation.
    if (TetradFieldOverlap(state)) {
        // could not rotate
        state->tetrad->rot = (state->tetrad->rot - rot + 4) % 4;
    } else if (state->do_rotate_timeout_reset) {
        state->tetrad->t = 0;
    }

    return;
}

void EventMove(STATE* state, int x)
{
    if (! state->tetrad || state->pause_f)
        return;

    state->tetrad->x += x;
    if (TetradFieldOverlap(state))
        state->tetrad->x -= x;

    return;
}

void EventTetrad(STATE* state)
{
    // NOTE: scoring for cleared lines occurs
    //       in LineClear();

    int y = 0, h = 0;

    // translate the tetrad to the field as tiles
    TetradTranslate(state, state->tetrad);
    // gather information about tetrad dimensions
    y = state->tetrad->y;
    h = state->tetrad->rot % 2 ? 4 : 2;
    // free the tetrad
    TetradFree(state->tetrad);
    state->tetrad = NULL;

    if (LineMark(state, y, h)) {
        state->line_clear_t = 0;
        state->line_clear_f = 1;
    } else {
        EventQuery(state);
    }

    /*
       TetradQueue(state);
       state->tetrad = ListDequeue(state->queue);
       */
    return;
}

void EventQuery(STATE* state)
{
    TetradQueue(state);
    state->tetrad = g_queue_pop_tail(state->queue);
    if (TetradFieldOverlap(state)) {
        state->game_over_f = 1;
    }

    return;
}

void EventPause(STATE* state)
{
    state->pause_f = 1;

    return;
}

void EventUnpause(STATE* state)
{
    state->pause_f = 0;

    return;
}

void DebugPrintTetrad(TETRAD* tetrad, FILE* file)
{
    fprintf(file, "%p", tetrad);
    return;
}

void StatusMessage(STATE* state, WINDOW* window, const char* str)
{
    int x, y, w, h, len;

    len = strlen(str);
    w = len + 2;
    h = 3;
    x = (state->Sx - w) / 2;
    y = (state->Sy - 3) / 2;

    BoxPrint(state->fieldwin, y, x, h, w);
    CarouselPrint(state, window, y + 1, x + 1, 3, str);

    wattrset(window, A_NORMAL);

    return;
}

void BoxPrint(WINDOW* window, int y, int x, int h, int w)
{
    int X = 0;
    int Y = 0;

    // print the top line
    for (Y = 0; Y < h; Y ++) {
        for (X = 0; X < w; X ++) {
            wmove(window, Y + y, X + x);

            if (Y == 0 || Y == h - 1) {
                if (X == 0) {
                    waddch(window, Y == 0 ? ACS_ULCORNER : ACS_LLCORNER);
                } else if (X == w - 1) {
                    waddch(window, Y == 0 ? ACS_URCORNER : ACS_LRCORNER);
                } else {
                    waddch(window, ACS_HLINE);
                }
            } else {
                waddch(window, ACS_VLINE);
                if (X == 0) {
                    X = w > 2 ? w - 2 : 0;
                }
            }
        }
    }

    return;
}

void CarouselPrint(STATE* state,
        WINDOW* window,
        int y,
        int x,
        int s,
        const char* str)
{
    size_t X = 0;
    int len = 0;

    len = strlen(str);

    wmove(window, y, x);
    for (X = 0; X < len; X++) {
        wattrset(window, A_NORMAL);
        wattron(window, COLOR_PAIR(((state->ticks / s) + X) % 8 + 20) |
                A_BOLD);
        waddch(window, str[X]);
    }

}

int Power(int a, int x)
{
    return x ? a * Power(a, x - 1) : 1;
}

int KeyParse(const char* str)
{
    unsigned char c;
    int i;

    if (str[0] == ':') {
        // numerical character
        sscanf(str, ":%c", &c);
        i = (unsigned char)c;
    } else {
        // read from string
        int X;

        for (X = 0; X <= TETRIS_MAX_KEYCODE; X++) {
            char buffer[TETRIS_BUFSIZE];

            // keyname(3X) sucks.
            if (keyname(X) != NULL) {
                strncpy(buffer, keyname(X), TETRIS_BUFSIZE);
                if (! strcmp(str, buffer)) {
                    // we found the key code!
                    i = X;
                    break;
                }
            }
        }

        if (X > TETRIS_MAX_KEYCODE) {
            // we could not find the key code. :(
            i = 0;
        }
    }

    return i;
}

