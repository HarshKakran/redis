#include "helper.h"

void die(const char *s) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

void msg(const char *s) {
    fprintf(stderr, "%s\n", msg);
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

void fd_set_nb(int fd){
    int flags = fcntl(fd, F_GETFL, 0); // get the flags
    flags |= O_NONBLOCK;               // modify the flags
    fcntl(fd, F_SETFL, flags);         // set the flags
}
