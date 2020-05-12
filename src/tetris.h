/*
 * ntetris: a tetris clone
 * (c) 2008 Lee Supe (lain_proliant)
 * Released under the GNU General Public License
 */

#include <curses.h>
#include <glib.h>

/*
 * The Tetrads
 * ####  #       #   ##   ##   #   ##
 *       ###   ###   ##  ##   ###   ##
 * (I)   (J)   (L)   (O) (S)  (T)  (Z)
 */

const char* shapes[] = {
    "####----#-#-#-#-####----#-#-#-#-", // I
    "#---###-###-#---###---#--#-###--", // J
    "--#-###-#-#-##--###-#---##-#-#--", // L
    "##--##--####----##--##--####----", // O
    "-##-##--#-##-#---##-##--#-##-#--", // S
    "-#--###-#-###---###--#---###-#--", // T
    "##---##--####---##---##--####---"  // Z
};

#define TETRIS_CLOCK_BUFSIZE    16
#define CLEARED                 127
#define REFRESH_DELAY           50
#define INIT_SPEED              1000/REFRESH_DELAY
#define DEFAULT_CLEAR_DELAY     500/REFRESH_DELAY
#define DELTA_SPEED             1
#define TETRIS_STATUS_HEIGHT    20
#define TETRIS_STATUS_WIDTH     17
#define TETRIS_KEYS             9
#define TETRIS_MAX_KEYCODE      410
#define TETRIS_BUFSIZE          256

const char* keymap_desc[] = {
    "quit",
    "drop",
    "lower",
    "rotcw",
    "rotccw",
    "left",
    "right",
    "pause",
    "reset"
};

enum {
    TETRIS_KEY_QUIT = 0,
    TETRIS_KEY_DROP = 1,
    TETRIS_KEY_LOWER = 2,
    TETRIS_KEY_ROTATE_CW = 3,
    TETRIS_KEY_ROTATE_CCW = 4,
    TETRIS_KEY_MOVE_LEFT = 5,
    TETRIS_KEY_MOVE_RIGHT = 6,
    TETRIS_KEY_PAUSE = 7,
    TETRIS_KEY_RESET = 8
};

static const char gs_appname[] = "ntetris-glib v2.0a";
static const char gs_gameover[] = " GAME OVER ";
static const char gs_pause[] = " PAUSE ";

enum {
    STATUS_GAMEOVER = 0,
    STATUS_GAME,
    STATUS_MENU
};

enum {
    CLEAR_NONE = 0,
    CLEAR_FLASH = 1,
    CLEAR_BLANK = 3
};

typedef struct _TETRAD {
    int shape;    // shape of tetrad
    int color;    // reserved
    int x, y;   // offset of tetrad
    int x0, y0; // previous offset of tetrad
    int t;       // time (ticks) since last update
    int rot;      // current rotation
} TETRAD;

typedef struct _STATE {
    GQueue* queue;
    char* field;

    WINDOW* fieldwin;
    WINDOW* statuswin;

    int keymap[9];

    /* settings */
    int init_level;
    int line_clear_timeout;

    unsigned long ticks;
    int line_clear_t;
    int line_clear_f;
    int game_over_f;
    int pause_f;

    TETRAD* tetrad;

    int status;
    int init_speed;
    int speed;
    int delta;
    int level;
    int lines;
    int score;
    int queue_size;

    char clock[TETRIS_CLOCK_BUFSIZE];

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

int ParseOptions(STATE*, int, char**);

STATE* Init(int, char**);
void InitTerminal(STATE*);
void DimensionWindows(STATE*);
void Reset(STATE*);
void Cleanup(STATE*);

void Paint(STATE*);
void Update(STATE*);
void Input(STATE*);
void Refresh(STATE*);

void StatusWindowPaint(STATE*);
int SignalHandler(int);

TETRAD* TetradAlloc(int, int, int);
TETRAD* TetradRandomAlloc(STATE* state);

void TetradFree(TETRAD*);
int TetradUpdate(STATE*);
void TetradQueue(STATE*);
void TetradPaint(WINDOW*, int, int, TETRAD*);
void TetradTranslate(STATE*, TETRAD*);
int TetradFieldOverlap(STATE*);
int TetradDrop(STATE*);

int RotateCorrection(TETRAD*);

int LineMark(STATE*, int y, int h);
void LineClear(STATE*);

void EventQuit(STATE*);
void EventDrop(STATE*);
void EventLower(STATE*);
void EventRotate(STATE*, int);
void EventMove(STATE*, int);
void EventTetrad(STATE*);
void EventQuery(STATE*);
void EventPause(STATE*);
void EventUnpause(STATE*);

void DebugPrintTetrad(TETRAD*, FILE*);

void StatusMessage(STATE*, WINDOW*, const char*);
void BoxPrint(WINDOW*, int, int, int, int);
void CarouselPrint(STATE*, WINDOW*, int, int, int, const char*);

int Power(int, int);
int KeyParse(const char*);
