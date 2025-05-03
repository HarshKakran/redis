#include "helper.h"

// function declarations
static bool try_one_request(Conn* conn);
static int32_t one_request(int conn_fd);
static void handle_read(Conn *conn);
static void handle_write(Conn *conn);
static Conn *handle_accept(int fd);

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

    // map of all the clients
    std::vector<Conn *> fd2conn;
    // the event loop 
    std::vector<struct pollfd> poll_args;

    while(true) {
        // prepare arguments for the poll call
        poll_args.clear();

        struct pollfd pfd;
        pfd.fd =  fd;
        pfd.events =  POLLIN;
        pfd.revents =  0;

        poll_args.push_back(pfd);

        for(Conn *conn:fd2conn) {
            if(!conn) {
                continue;
            }

            struct pollfd pfd;
            pfd.fd = conn->fd; 
            pfd.events = POLLERR; 
            pfd.revents = 0;


            if(conn->want_read) {
                pfd.events |= POLLIN;
            }

            if(conn->want_write) {
                pfd.events |= POLLOUT;
            }

            poll_args.push_back(pfd);

        }

        // wait for readiness
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
        if(rv < 0 && errno == EINTR) {
            continue;   // not an error
        } 
        if (rv < 0){
            die("poll");
        }
        
        // handle the listening sockets
        if (poll_args[0].revents) {
            if (Conn *conn = handle_accept(fd)) {
                // put it in the map
                if (fd2conn.size() <= (size_t)conn->fd)  {
                    fd2conn.resize(conn->fd + 1);
                }
                fd2conn[conn->fd] = conn;
            }
        }

        // handle connection sockets
        for(size_t i=1; i<poll_args.size(); ++i)  {
            uint8_t ready = poll_args[i].revents;
            Conn *conn = fd2conn[poll_args[i].fd];
            
            if(ready & POLLIN) {
                handle_read(conn);
            }
            if (ready & POLLOUT) {
                handle_write(conn);
            }
            // close the socket if there is an error or according to application logic
            if ((ready & POLLERR) || conn->want_close) {
                (void)close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
        }
    }

    return 0;
}

static bool try_one_request(Conn* conn) {
    // 3. Try to parse the accumulated buffer.
    // protocol: message header
    if(conn->incoming.size() < 4) {
        return false; // want read
    }

    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (len > k_max_msg) { // protocol error: message too long
        msg("protocol error: message too long\n");
        conn->want_close = true;
        return false; 
    }

    // protocol message body
    if(len + 4 > conn->incoming.size()) {
        return false; // want read
    }

    const uint8_t *request = &conn->incoming[4];
    // 4. Process the parsed message.
    printf("len:%u data:%.*s\n", len, len < 100 ? len : 100, request);

    // generate the response (echo)
    buf_append(conn->outgoing, (const uint8_t *)& len, 4);
    buf_append(conn->outgoing, request, len);
    
    // 5. Remove message from `conn::incoming`.
    // conn->incoming.clear()               // WRONG
    buf_consume(conn->incoming, 4 + len);   // CORRECT
    return true; // successbrew install --cask docker

}

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

static Conn *handle_accept(int fd) {
    // accept
    struct sockaddr_in client = {};
    socklen_t addrlen = sizeof(client);
    int conn_fd =   accept(fd, (sockaddr *)& client, &addrlen);
    if (conn_fd < 0) {
        return NULL;
    }

    // set the new connection fd to non blocking
    fd_set_nb(conn_fd);

    // create a struct Conn
    Conn *conn = new Conn();
    conn->fd  =  conn_fd;
    conn->want_read = true; // read the 1st request
    // TODO: Initialise buffers
    return conn;
}

static void handle_read(Conn *conn) {
    // 1. Do a non blocking read 
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if (rv <= 0) { // handle IO error (rv < 0) or EOF (rv == 0)
        conn->want_close = true;
        return;
    }

    // 2. Add the read data to the incoming buffer
    buf_append(conn->incoming, buf, (size_t)rv);
    // 3. Try to parse the accumulated buffer.
    // 4. Process the parsed message.
    // 5. Remove the message from `Conn::incoming`.
    // try_one_request(conn);               // WRONG
    while (try_one_request(conn)) {}        // CORRECT

    if(conn->outgoing.size() > 0) { // has a response
        conn->want_read = false;
        conn->want_write = true;
        // The socket is likely ready to write in a request-response protocol,
        // try to write it without waiting for the next iteration.
        return handle_write(conn);      // optimization
    } // else read more data
}

static void handle_write(Conn *conn){
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
    if (rv < 0 && errno==EAGAIN) {
        return; // actually not ready
    }
    if (rv < 0) {
        conn->want_close = true;
        return;
    }

    // remove written data 
    buf_consume(conn->outgoing, (size_t)rv);

    if (conn->outgoing.size() == 0) {
        conn->want_write = false;
        conn->want_read = true;
    } // else write
}