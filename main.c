#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <threads.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>

enum State {
    STATE_SPIN,
    STATE_EXIT,
};

struct ThreadCtx {
    thrd_t  job;
    size_t  sz;
    bool    finished;
    int     index;
};

static _Atomic(int) state = 0;
static int threads_num = 0;
static struct ThreadCtx threads[100];

static int job(void* arg) {
    struct ThreadCtx * ctx = arg;
    printf("job: %d\n", ctx->index);
    //size_t sz = 1024u * 1024u * 1024u * 10;
    //printf("sizeof(sz) %zu\n", sizeof(sz));
    void *ptr = mmap(
            NULL, ctx->sz,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            0, 0
            );

    if (ptr == MAP_FAILED) {
        printf("error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < ctx->sz; ++i) {
        ((char*)ptr)[i] = i % 10;
    }
    printf("index %d, ptr %p\n", ctx->index, ptr);

    for(;;) {
        if (state == STATE_EXIT)
            break;
        usleep(1000);
    }

    printf("job: munmap\n");
    munmap(ptr, ctx->sz);
    ctx->finished = true;
    return 0;
}

static void sig_handler(int n) {
    printf("sig_handler: %d\n", n);

    state = STATE_EXIT;
    bool all_finished = true;
    int max_iter = 70000;
    while (!all_finished) {
        max_iter--;
        for (int i = 0; i < threads_num; i++) {
            all_finished &= threads[i].finished;
        }
        if (all_finished)
            return;
        usleep(100);
        if (max_iter <= 0) {
            printf("max_iter reached zero, shutdown\n");
            exit(EXIT_FAILURE);
        }
    }

    printf("threads were fihished\n");
}

int main(int argc, char **argv) {
    //printf("argc %d\n", argc);
    //printf("agrv[0] %s\n", argv[0]);
    if (argc != 2) {
        printf("there is no one integer argument - allocating size in GB\n");
        exit(EXIT_FAILURE);
    }

    sscanf(argv[1], "%d", &threads_num);
    if (threads_num < 0) {
        printf("threads_num < 0\n");
        exit(EXIT_FAILURE);
    }

    printf("threads_num %d\n", threads_num);

    signal(SIGINT, sig_handler);
    state = STATE_SPIN;
    for (int i = 0; i < threads_num; i++) {
        threads[i] = (struct ThreadCtx) {
            .sz = 1024 * 1024 * 1024,
            .job = 0,
            .finished = false,
            .index = i,
        };
        thrd_create(&threads[i].job, job, &threads[i]);
    }

    while(true) {
        if (state == STATE_EXIT)
            break;
        usleep(100);
    }
}
