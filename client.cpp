#include "helper.h"

static int32_t send_req(int fd, const std::vector<std::string> &cmd)
{
    uint32_t len = 4;
    for (const std::string &s : cmd)
    {
        len += 4 + s.size();
    }

    if (len > k_max_msg)
    {
        return -1;
    }

    char wbuf[4 + k_max_msg];
    memcpy(&wbuf[0], &len, 4);
    uint32_t n = (uint32_t)cmd.size();
    memcpy(&wbuf[4], &n, 4);
    size_t cur = 8;
    for (const std::string &s : cmd)
    {
        uint32_t p = (uint32_t)s.size();
        memcpy(&wbuf[cur], &p, 4);
        memcpy(&wbuf[cur + 4], s.data(), s.size());
        cur += 4 + s.size();
    }

    return write_all(fd, wbuf, 4 + len);
}

static int32_t print_response(const uint8_t *data, size_t size)
{
    if (size < 1)
    {
        msg("bad response");
        return -1;
    }

    switch (data[0])
    {
    case TAG_NIL:
        printf("(nil)\n");
        return 1;
    case TAG_ERR:
        if (size < 1 + 8)
        {
            msg("bad response");
            return -1;
        }

        {
            int32_t code = 0;
            uint32_t len = 0;
            memcpy(&code, &data[1], 4);
            memcpy(&len, &data[5], 4);

            if (size < 1 + 8 + len)
            {
                msg("bad response");
                return -1;
            }

            printf("(err) %d %. %s\n", code, len, &data[1 + 8]);
            return 1 + 8 + len;
        }

    case TAG_STR:
        if (size < 1 + 4)
        {
            msg("bad response");
            return -1;
        }
        {
            uint32_t len = 0;
            memcpy(&len, &data[1], 4);

            if (size < 1 + 4 + len)
            {
                msg("bad response");
                return -1;
            }

            printf("(str) %. %s\n", len, &data[1 + 4]);
            return 1 + 4 + len;
        }

    case TAG_INT:
        if (size < 1 + 8)
        {
            msg("bad response");
            return -1;
        }

        {
            int64_t val = 0;
            memcpy(&val, &data[1], 8);
            printf("(int) %ld\n", &val);
            return 1 + 8;
        }

    case TAG_DBL:
        if (size < 1 + 8)
        {
            msg("bad response");
            return -1;
        }
        {
            double val = 0;
            memcpy(&val, &data[1], 8);
            printf("(dbl) %g\n", val);
            return 1 + 8;
        }

    case TAG_ARR:
        if (size < 1 + 4)
        {
            msg("bad response");
            return -1;
        }

        {
            uint32_t len = 0;
            memcpy(&len, &data[1], 4);
            printf("(arr) len=%u\n", len);
            size_t arr_bytes = 1 + 4;
            for (uint32_t i = 0; i < len; ++i)
            {
                int32_t rv = print_response(&data[arr_bytes], size - arr_bytes);
                if (rv < 0)
                {
                    return rv;
                }
                arr_bytes += (size_t)rv;
            }
            printf("(arr) end\n");
            return (int32_t)arr_bytes;
        }

    default:
        msg("bad response");
        return -1;
    }
}

static int32_t read_res(int fd)
{
    // 4 bytes header
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err)
    {
        if (errno == 0)
        {
            msg("EOF");
        }
        else
        {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4); // assume little endian
    if (len > k_max_msg)
    {
        msg("too long");
        return -1;
    }

    err = read_full(fd, &rbuf[4], len);
    if (err)
    {
        msg("read() error");
        return err;
    }

    // print the result
    int32_t rv = print_response((uint8_t *)&rbuf[4], len);
    if (rv > 0 && (uint32_t)rv != len)
    {
        msg("bad response");
        rv = -1;
    }
    return rv;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <server-container> [port=1234]\n", argv[0]);
        return 1;
    }

    const char *hostname = argv[1]; // server's container name
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
    if (rv != 0)
    {
        die("getaddrinfo");
    }

    // Loop through results and connect to the first we can
    for (p = res; p != NULL; p = p->ai_next)
    {
        char ipstr[INET_ADDRSTRLEN];
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        inet_ntop(AF_INET, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
        printf("Trying %s:%s...\n", ipstr, port);

        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0)
        {
            int err = errno;
            fprintf(stderr, "[%d] %s %d\n", err, "socket", fd);
            continue;
        }

        int rv = connect(fd, p->ai_addr, p->ai_addrlen);
        if (rv)
        {
            int err = errno;
            fprintf(stderr, "[%d] %s\n", err, "socket");
            continue;
        }

        break;
    }

    freeaddrinfo(res);
    if (p == NULL || fd < 0)
    {
        die("client: failed to connect");
    }

    // multiple pipelined requests
    std::vector<std::string> cmd;
    for (int i = 1; i < argc; ++i)
    {
        cmd.push_back(argv[i]);
    }
    int32_t err = send_req(fd, cmd);
    if (err)
    {
        goto L_DONE;
    }
    err = read_res(fd);
    if (err)
    {
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;
}