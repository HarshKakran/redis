#include "helper.h"

static int32_t read_full(int fd, uint8_t *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const uint8_t *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// the `query` function was simply splited into `send_req` and `read_res`.
static int32_t send_req(int fd, const uint8_t *text, size_t len) {
    if (len > k_max_msg) {
        return -1;
    }

    std::vector<uint8_t> wbuf;
    buf_append(wbuf, (const uint8_t *)&len, 4);
    buf_append(wbuf, text, len);
    return write_all(fd, wbuf.data(), wbuf.size());
}

static int32_t read_res(int fd) {
    // 4 bytes header
    std::vector<uint8_t> rbuf;
    rbuf.resize(4);
    errno = 0;
    int32_t err = read_full(fd, &rbuf[0], 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf.data(), 4);  // assume little endian
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    // reply body
    rbuf.resize(4 + len);
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    // do something
    printf("len:%u data:%.*s\n", len, len < 100 ? len : 100, &rbuf[4]);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server-container> [port=1234]\n", argv[0]);
        return 1;
    }

    const char *hostname = argv[1];  // server's container name
    const char *port = (argc > 2) ? argv[2] : "1234";
    struct addrinfo hints, *res, *p;
    int fd = -1;
    int rv;

    // Setup hints for getaddrinfo
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Resolve the hostname and port
    rv = getaddrinfo(hostname, port, &hints, &res);
    if (rv != 0) {
        die("getaddrinfo");
    }

    // Loop through results and connect to the first we can
    for(p = res; p != NULL; p = p->ai_next) {
        char ipstr[INET_ADDRSTRLEN];
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        inet_ntop(AF_INET, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
        printf("Trying %s:%s...\n", ipstr, port);
        
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) {
            int err = errno;
            fprintf(stderr, "[%d] %s %d\n", err, "socket", fd);
            continue;
        }
    
        int rv = connect(fd, p->ai_addr, p->ai_addrlen);
        if (rv) {
            int err = errno;
            fprintf(stderr, "[%d] %s\n", err, "socket");
            continue;
        }

        break;
    }

    freeaddrinfo(res);
    if (p == NULL || fd < 0) {
        die("client: failed to connect");
    }

    // multiple pipelined requests
    std::vector<std::string> query_list = {
        "hello1", "hello2", "hello3",
        // a large message requires multiple event loop iterations
        std::string(k_max_msg, 'z'),
        "hello5",
    };
    for (const std::string &s : query_list) {
        int32_t err = send_req(fd, (uint8_t *)s.data(), s.size());
        if (err) {
            goto L_DONE;
        }
    }
    for (size_t i = 0; i < query_list.size(); ++i) {
        int32_t err = read_res(fd);
        if (err) {
            goto L_DONE;
        }
    }

L_DONE:
    close(fd);
    return 0;
}