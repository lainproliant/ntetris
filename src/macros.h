#pragma once

#ifndef __MACROS_H
#define __MACROS_H
/* warning and error macros */

/* The proper way to do this is probably terminal control sequences :-/ */

#define RESET "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */
#define WARNCOLOR 11
#define ERRCOLOR 22

#define ERROR(fmt, ...) \
        do { \
            fprintf(stderr, BOLDRED "[ERROR] %s:%d:%s(): " fmt "\n" RESET, __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); exit(-1); \
        } while (0)

#define WARNING(fmt, ...) \
        do { fprintf(stdout, BOLDYELLOW "[WARNING] %s:%d:%s(): " fmt "\n" RESET, \
                 __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        } while (0)

#define PRINT(...) printf(__VA_ARGS__)

#define WARN(...) WARNING("%s", __VA_ARGS__);
#define ERRMSG(...) ERROR("%s", __VA_ARGS__);

#endif
