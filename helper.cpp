#include "helper.h"

void die(const char *s) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, s);
    abort();
}

void msg(const char *s) {
    fprintf(stderr, "%s\n", s);
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

// append at the back
void buf_append(std::vector<uint8_t>& buf, const uint8_t* data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}

// remove from the front
void buf_consume(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}
