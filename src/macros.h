#pragma once
/* warning and error macros */

/* The proper way to do this is probably terminal control sequences :-/ */
#define WARNCOLOR 1
#define ERRCOLOR 2
#include <ncurses.h>

#ifdef NCURSES

#define ERROR(fmt, ...) \
        do { \
            wattron(mainWindow, A_BOLD | COLOR_PAIR(ERRCOLOR)); \
            wprintw(mainWindow, "%s:%d:%s(): " fmt "\n", __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); exit(-1); \
            wattrset(mainWindow, A_NORMAL); \
        } while (0)

#define WARNING(fmt, ...) \
        do { \
            wattron(mainWindow, A_BOLD | COLOR_PAIR(WARNCOLOR)); \
            wprintw(mainWindow, "%s:%d:%s(): " fmt "\n", __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); \
            wattrset(mainWindow, A_NORMAL); \
        } while (0)
#else

#define ERROR(fmt, ...) \
        do { \
            fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); exit(-1); \
        } while (0)

#define WARNING(fmt, ...) \
        do { fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); \
        } while (0)
#endif


#define WARN(msg) WARNING("%s", msg);
#define ERRMSG(msg) ERROR("%s", msg);
