#include "helper.h"
#include "hashtable.h"

// member is the attribute name in type T
#define container_of(ptr, T, member) \
    ((T *)((char *)ptr - offsetof(T, member)))

// function declarations
static bool try_one_request(Conn *conn);
static void handle_read(Conn *conn);
static void handle_write(Conn *conn);
static Conn *handle_accept(int fd);
static int32_t parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out);
static void do_request(std::vector<std::string> &cmd, Buffer &out);
static void do_del(std::vector<std::string> &cmd, Buffer &out);
static void do_set(std::vector<std::string> &cmd, Buffer &out);
static void do_get(std::vector<std::string> &cmd, Buffer &out);
static void do_keys(std::vector<std::string> &cmd, Buffer &out);
static bool cb_keys(HNode *node, void *args);

static void response_begin(Buffer &out, size_t *header);
static size_t response_size(Buffer &out, size_t header);
static void response_end(Buffer &out, size_t header);

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);     // port
    addr.sin_addr.s_addr = htonl(0); // wildcard IP 0.0.0.0

    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv)
    {
        // perror("bind");
        // exit(EXIT_FAILURE);
        die("bind");
    }

    fd_set_nb(fd);

    rv = listen(fd, SOMAXCONN);
    if (rv)
    {
        die("listen");
    }

    // map of all the clients
    std::vector<Conn *> fd2conn;
    // the event loop
    std::vector<struct pollfd> poll_args;

    while (true)
    {
        // prepare arguments for the poll call
        poll_args.clear();

        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        poll_args.push_back(pfd);

        for (Conn *conn : fd2conn)
        {
            if (!conn)
            {
                continue;
            }

            struct pollfd pfd;
            pfd.fd = conn->fd;
            pfd.events = POLLERR;
            pfd.revents = 0;

            if (conn->want_read)
            {
                pfd.events |= POLLIN;
            }

            if (conn->want_write)
            {
                pfd.events |= POLLOUT;
            }

            poll_args.push_back(pfd);
        }

        // wait for readiness
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
        if (rv < 0 && errno == EINTR)
        {
            continue; // not an error
        }
        if (rv < 0)
        {
            die("poll");
        }

        // handle the listening sockets
        if (poll_args[0].revents)
        {
            if (Conn *conn = handle_accept(fd))
            {
                // put it in the map
                if (fd2conn.size() <= (size_t)conn->fd)
                {
                    fd2conn.resize(conn->fd + 1);
                }
                fd2conn[conn->fd] = conn;
            }
        }

        // handle connection sockets
        for (size_t i = 1; i < poll_args.size(); ++i)
        {
            uint8_t ready = poll_args[i].revents;
            Conn *conn = fd2conn[poll_args[i].fd];

            if (ready & POLLIN)
            {
                handle_read(conn);
            }
            if (ready & POLLOUT)
            {
                handle_write(conn);
            }
            // close the socket if there is an error or according to application logic
            if ((ready & POLLERR) || conn->want_close)
            {
                (void)close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
        }
    }

    return 0;
}

static bool try_one_request(Conn *conn)
{
    // 3. Try to parse the accumulated buffer.
    // protocol: message header
    if (conn->incoming.size() < 4)
    {
        return false; // want read
    }

    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (len > k_max_msg)
    { // protocol error: message too long
        msg("protocol error: message too long\n");
        conn->want_close = true;
        return false;
    }

    // protocol message body
    if (len + 4 > conn->incoming.size())
    {
        return false; // want read
    }

    const uint8_t *request = &conn->incoming[4];
    // 4. Process the parsed message.
    std::vector<std::string> cmd;
    if (parse_req(request, len, cmd) < 0)
    {
        conn->want_close = true;
        return false;
    }

    size_t header_pos = 0;
    response_begin(conn->outgoing, &header_pos);
    do_request(cmd, conn->outgoing);
    response_end(conn->outgoing, header_pos);

    // 5. Remove message from `conn::incoming`.
    // conn->incoming.clear()               // WRONG
    buf_consume(conn->incoming, 4 + len); // CORRECT
    return true;                          // success
}

static Conn *handle_accept(int fd)
{
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int conn_fd = accept(fd, (sockaddr *)&client_addr, &addrlen);
    if (conn_fd < 0)
    {
        msg_errno("accept() error");
        return NULL;
    }

    uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "new client from %u.%u.%u.%u:%u\n%u\n",
            ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24,
            ntohs(client_addr.sin_port), ip);

    // set the new connection fd to non blocking
    fd_set_nb(conn_fd);

    // create a struct Conn
    Conn *conn = new Conn();
    conn->fd = conn_fd;
    conn->want_read = true; // read the 1st request
    // TODO: Initialise buffers
    return conn;
}

static void handle_read(Conn *conn)
{
    // 1. Do a non blocking read
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if (rv < 0 && errno == EAGAIN)
    {
        return; // actually not ready
    }
    if (rv < 0)
    { // handle IO error (rv < 0) or EOF (rv == 0)
        msg_errno("read() error");
        conn->want_close = true;
        return;
    }
    if (rv == 0)
    { // EOF
        if (conn->incoming.size() == 0)
        {
            msg("client closed");
        }
        else
        {
            msg("unexpected EOF");
        }
        conn->want_close = true;
        return; // want close
    }

    // 2. Add the read data to the incoming buffer
    buf_append(conn->incoming, buf, (size_t)rv);

    // try_one_request(conn);               // WRONG
    while (try_one_request(conn))
    {
    } // CORRECT

    if (conn->outgoing.size() > 0)
    { // has a response
        conn->want_read = false;
        conn->want_write = true;
        // The socket is likely ready to write in a request-response protocol,
        // try to write it without waiting for the next iteration.
        return handle_write(conn); // optimization
    } // else read more data
}

static void handle_write(Conn *conn)
{
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
    if (rv < 0 && errno == EAGAIN)
    {
        return; // actually not ready
    }
    if (rv < 0)
    {
        msg("write() error");
        conn->want_close = true;
        return;
    }

    // remove written data
    buf_consume(conn->outgoing, (size_t)rv);

    if (conn->outgoing.size() == 0)
    {
        conn->want_write = false;
        conn->want_read = true;
    } // else write
}

// +------+-----+------+-----+------+-----+-----+------+
// | nstr | len | str1 | len | str2 | ... | len | strn |
// +------+-----+------+-----+------+-----+-----+------+

static int32_t parse_req(const uint8_t *data, size_t size, std::vector<std::string> &out)
{
    const uint8_t *end = data + size;
    uint32_t nstr = 0;
    if (!read_u32(data, end, nstr))
    {
        return -1;
    }
    if (nstr > k_max_msg)
    {
        return -1;
    }

    while (out.size() < nstr)
    {
        uint32_t len = 0;
        if (!read_u32(data, end, len))
        {
            return -1;
        }
        out.push_back(std::string());
        if (!read_str(data, end, len, out.back()))
        {
            return -1;
        }
    }

    if (data != end)
    {
        return -1;
    }

    return 0;
}

// static std::map<std::string, std::string> g_data;

static void do_request(std::vector<std::string> &cmd, Buffer &out)
{
    if (cmd.size() == 2 && cmd[0] == "get")
    {
        return do_get(cmd, out);
    }
    else if ((cmd.size() == 3) && (cmd[0] == "set"))
    {
        return do_set(cmd, out);
    }
    else if (cmd.size() == 2 && cmd[0] == "del")
    {
        return do_del(cmd, out);
    }
    else if (cmd.size() == 1 && cmd[0] == "keys")
    {
        return do_keys(cmd, out);
    }
    else
    {
        out_err(out, ERR_UNKNOWN, "unknown command."); // unrecognized command
    }
}

static void response_begin(Buffer &out, size_t *header)
{
    *header = out.size();   // message header position
    buf_append_u32(out, 0); // reserving the space
}

static size_t response_size(Buffer &out, size_t header)
{
    return out.size() - header - 4;
}

static void response_end(Buffer &out, size_t header)
{
    size_t msg_size = out.size() - header;
    if (msg_size > k_max_msg)
    {
        out.resize(header + 4);
        out_err(out, ERR_TOO_BIG, "too big response.");
        msg_size = response_size(out, header);
    }
    // message header - added response size
    uint32_t len = (uint32_t)msg_size;
    memcpy(&out[header], &len, 4);
}

// gloabl states
static struct
{
    HMap db;
} g_data;

struct Entry
{
    struct HNode node;
    std::string key;
    std::string val;
};

// equality comparison for struct `Entry`
static bool eq_entry(HNode *lhs, HNode *rhs)
{
    Entry *l = container_of(lhs, Entry, node);
    Entry *r = container_of(rhs, Entry, node);

    return l->key == r->key;
}

// FNV hash
static uint64_t str_hash(const uint8_t *data, size_t len)
{
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++)
    {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

static void do_get(std::vector<std::string> &cmd, Buffer &out)
{
    // create a key node for look up
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    // hash table lookup
    HNode *node = hm_lookup(&g_data.db, &key.node, &eq_entry);
    if (!node)
    {
        return out_nil(out);
    }

    // copy the value in output
    const std::string val = container_of(node, Entry, node)->val;
    return out_str(out, val.data(), val.size());
}

static void do_set(std::vector<std::string> &cmd, Buffer &out)
{
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &eq_entry);
    if (node)
    {
        container_of(node, Entry, node)->val.swap(cmd[2]);
    }
    else
    {
        Entry *e = new Entry;
        e->key.swap(key.key);
        e->node.hcode = key.node.hcode;
        e->val.swap(cmd[2]);
        hm_insert(&g_data.db, &e->node);
    }

    return out_nil(out);
}

static void do_del(std::vector<std::string> &cmd, Buffer &out)
{
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_delete(&g_data.db, &key.node, &eq_entry);
    return out_int(out, node ? 1 : 0);
}

static bool cb_keys(HNode *node, void *args)
{
    Buffer &out = *(Buffer *)args;
    const std::string &key = container_of(node, Entry, node)->key;
    out_str(out, key.data(), key.size());
    return true;
}

static void do_keys(std::vector<std::string> &cmd, Buffer &out)
{
    out_arr(out, uint32_t(hm_size(&g_data.db)));
    hm_foreach(&g_data.db, &cb_keys, (void *)&out);
}
