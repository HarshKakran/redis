#include "helper.h"

static int32_t one_request(int conn_fd) {
    // 4 bytes header
    char rbuf[4 + k_max_msg];
    // errno is set to the error code if the syscall failed. 
    // However, errno is NOT set to 0 if the syscall succeeded; 
    // it simply keeps the previous value. 
    // That’s why the following code sets errno = 0 before read_full() to distinguish the EOF case.
    errno = 0;
    int32_t err = read_full(conn_fd, rbuf, 4);
    if (err) {
        msg(errno == 0 ? "EOF": "read() error");
        return err; 
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4); // this will copy the first 4 byte at the memory address pointed by rbuf
    if(len > k_max_msg) {
        msg("too long");
    }

    // request body, after the 4 byte header
    err = read_full(conn_fd, &rbuf[4], len);
    if(err) {
        msg("read() error");
        return err;
    }

    printf("client says: %.*s\n", len, &rbuf[4]);

    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(conn_fd, wbuf, 4 + len);
}

// static void do_something(int fd) {
//     char rbuf[64] = {};
//     ssize_t n = read(fd, rbuf, sizeof(rbuf)-1);
//     if(n < 0) {
//         msg("read() error");
//         return;
//     } 

//     printf("client says: %s\n", rbuf);

//     char wbuf[] = "world";
//     write(fd, wbuf, strlen(wbuf));
// }

int main() {    
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);        // port
    addr.sin_addr.s_addr = htonl(0);    // wildcard IP 0.0.0.0

    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        // perror("bind");
        // exit(EXIT_FAILURE);
        die("bind");
    }

    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen");
    }

    while(true) {
        // accept the request
        struct sockaddr_in client = {};
        socklen_t addrlen = sizeof(client);
        int conn_fd =   accept(fd, (sockaddr *)& client, &addrlen);
        if (conn_fd < 0) {
            continue;
        }

        // do_something(conn_fd);
        // only serves one client connection at once
        while(true) {
            int32_t err = one_request(conn_fd);
            if(err) {
                break;
            }
        }

        close(conn_fd);
    }


    return 0;
}