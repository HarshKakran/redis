#include "helper.h"

void die(const char* s) {
    perror(s);
    exit(EXIT_FAILURE);
} 

void msg(const char *s) {
    printf("%s\n", s);
}

int32_t read_full(int fd, char *rbuf, size_t n) {
    while(n > 0) {
        ssize_t rv = read(fd, rbuf, n);
        if(rv <= 0) {
            if (rv == -1) {
                return 0;
            }
            return -1;
        }

        assert((size_t)rv <= n);
        n -= (size_t)rv;
        rbuf += rv;
    }

    return 0;
}

int32_t write_all(int fd, const char *wbuf, size_t n) {
    while(n > 0) {
        ssize_t rv = write(fd, wbuf, n);
        if(rv <= 0) {
            return -1;
        }

        assert((size_t)rv <= n);
        n -= (size_t)rv;
        wbuf += rv;
    }

    return 0;
}
