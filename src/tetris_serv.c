#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#ifdef __sun
#include "strtonum.h"
#include <mtmalloc.h>
#else
#include <strtonum.h>
#endif

#include <limits.h>
#include <uv.h>
#include "packet.h"

#define DEFAULT_PORT 48879

#define ERROR(fmt, ...) \
        do { fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, \
                     __LINE__, __func__, ##__VA_ARGS__); exit(-1); \
        } while (0)

#define ERR(msg) ERROR("%s", msg);

static void malloc_cb(uv_handle_t *h, size_t s, uv_buf_t *b)
{
    b->base = (char*)malloc(s * sizeof(char));
    assert(b->base != NULL);
    b->len = s;
}

int main(int argc, char *argv[])
{
    int go_ret;
    int port = DEFAULT_PORT;
    const char *err_str = NULL;

    static struct option longopts[] = {
        {"port",      required_argument,     NULL,     'p'},
        {NULL,        0,                     NULL,     0}
    };

    while ((go_ret = getopt_long(argc, argv, "p:", longopts, NULL)) != -1) {
       switch (go_ret) {
            case 'p':
                port = strtonum(optarg, 1, UINT16_MAX, &err_str);
                if (err_str) {
                    ERR("Bad value for port");
                }
       }
    }

    loop = uv_default_loop();
    uv_udp_t recv_sock;
    uv_udp_init(loop, &recv_sock); 
    struct sockaddr_in recaddr;

    uv_ip4_addr("0.0.0.0", 8000, &recaddr);
    uv_udp_bind(&recv_sock, (const struct sockaddr*)&recaddr, UV_UDP_REUSEADDR);
    uv_udp_recv_start(&recv_sock, malloc_cb, onrecv);

    return uv_run(loop, UV_RUN_DEFAULT);
}
