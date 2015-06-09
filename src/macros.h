#pragma once
/* warning and error macros */

#include <ncurses.h>

#define ERROR(fmt, ...) \
        do { \
            wattron(mainWindow, A_BOLD | COLOR_RED); \
            fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); exit(-1); \
            wattrset(mainWindow, A_NORMAL);\
        } while (0)

#define ERRMSG(msg) ERROR("%s", msg);

#define WARNING(fmt, ...) \
        do { fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); \
        } while (0)

/*#define WARNING(fmt, ...) \
        do { \
            wattron(mainWindow, A_BOLD | COLOR_YELLOW); \
            wprintw(mainWindow, "%s:%d:%s(): " fmt "\n", __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); \
            wattrset(mainWindow, A_NORMAL);\
        } while (0)*/

#define WARN(msg) WARNING("%s", msg);
