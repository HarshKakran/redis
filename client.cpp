#include "helper.h"


static int32_t query(int fd, const char *text) {
    uint32_t len = (uint32_t)strlen(text);
    if(len > k_max_msg) {
        return -1;
    }

    // send request
    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    if(int32_t err = write_all(fd, wbuf, 4 + len)) {
        return err;
    }

    // 4 bytes header
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if(err) {
        msg(errno == 0 ? "EOF": "read() error");
        return err;
    }

    memcpy(&len, rbuf, 4); // assuming little endian
    if(len > k_max_msg) {
        msg("too long");
        return -1;
    }

    // reply body
    err = read_full(fd, &rbuf[4], len);
    if(err) {
        msg("read() error");
        return err;
    }

    printf("server says: %.*s\n", len, &rbuf[4]);
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    int rv = connect(fd, (const sockaddr *) &addr, sizeof(addr));
    if (rv) {
        die("connect()");
    }

    // char msg[] = "hello";
    // write(fd, msg, strlen(msg));

    // char rbuf[64] = {};
    // ssize_t n = read(fd, rbuf, sizeof(rbuf)-1);
    // if (n < 0) {
    //     die("read()");
    // }

    // printf("server says: %s\n", rbuf);

     // send multiple requests
     int32_t err = query(fd, "hello1");
     if (err) {
         goto L_DONE;
     }
     err = query(fd, "hello2");
     if (err) {
         goto L_DONE;
     }
 L_DONE:
    close(fd);


    return 0;
}