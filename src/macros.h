#pragma once
/* warning and error macros */

#define ERROR(fmt, ...) \
        do { fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); exit(-1); \
        } while (0)

#define ERR(msg) ERROR("%s", msg);

#define WARNING(fmt, ...) \
        do { fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); \
        } while (0)

#define WARN(msg) WARNING("%s", msg);
