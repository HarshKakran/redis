#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>


void die(const char* s) {
    perror(s);
    exit(EXIT_FAILURE);
} 

void msg(const char *s) {
    printf("%s\n", s);
}

static void do_something(int fd) {
    char rbuf[64] = {};
    ssize_t n = read(fd, rbuf, sizeof(rbuf)-1);
    if(n < 0) {
        msg("read() error");
        return;
    } 

    printf("client says: %s\n", rbuf);

    char wbuf[] = "world";
    write(fd, wbuf, strlen(wbuf));
}

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
        struct sockaddr_in client = {};
        socklen_t addrlen = sizeof(client);
        int conn_fd =   accept(fd, (sockaddr *)& client, &addrlen);
        if (conn_fd < 0) {
            continue;
        }

        do_something(conn_fd);
        close(conn_fd);
    }


    return 0;
}