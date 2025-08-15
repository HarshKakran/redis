#include "helper.h"

void die(const char *s)
{
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, s);
    abort();
}

void msg(const char *s)
{
    fprintf(stderr, "%s\n", s);
}

void msg_errno(const char *msg)
{
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

int32_t read_full(int fd, char *rbuf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = read(fd, rbuf, n);
        if (rv <= 0)
        {
            if (rv == -1)
            {
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

int32_t write_all(int fd, const char *wbuf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = write(fd, wbuf, n);
        if (rv <= 0)
        {
            return -1;
        }

        assert((size_t)rv <= n);
        n -= (size_t)rv;
        wbuf += rv;
    }

    return 0;
}

void fd_set_nb(int fd)
{
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0); // get the flags
    if (errno)
    {
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK; // modify the flags

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags); // set the flags
    if (errno)
    {
        die("fcntl error");
        return;
    }
}

// append at the back
void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len)
{
    buf.insert(buf.end(), data, data + len);
}

// remove from the front
void buf_consume(std::vector<uint8_t> &buf, size_t n)
{
    buf.erase(buf.begin(), buf.begin() + n);
}

void buf_append_u8(Buffer &buf, uint8_t data)
{
    buf.push_back(data);
}

void buf_append_u32(Buffer &buf, uint32_t data)
{
    buf_append(buf, (const uint8_t *)&data, 4); // assume littlen-endian
}

void buf_append_i64(Buffer &buf, int64_t data)
{
    buf_append(buf, (const uint8_t *)&data, 8);
}

void buf_append_dbl(Buffer &buf, double data)
{
    buf_append(buf, (const uint8_t *)&data, 8);
}

bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out)
{
    if (cur + 4 > end)
    {
        return false;
    }

    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out)
{
    if (cur + n > end)
    {
        return false;
    }

    out.assign(cur, cur + n);
    cur += n;
    return true;
}

void out_nil(Buffer &out)
{
    buf_append_u8(out, TAG_NIL);
}

void out_str(Buffer &out, const char *s, size_t size)
{
    buf_append_u8(out, TAG_STR);
    buf_append_u32(out, (uint32_t)size);
    buf_append(out, (const uint8_t *)s, size);
}
void out_int(Buffer &out, int64_t val)
{
    buf_append_u8(out, TAG_INT);
    buf_append_i64(out, val);
}

void out_dbl(Buffer &out, double val)
{
    buf_append_u8(out, TAG_DBL);
    buf_append_dbl(out, val);
}

void out_err(Buffer &out, uint32_t code, const std::string &msg)
{
    buf_append_u8(out, TAG_ERR);
    buf_append_u32(out, code);
    buf_append_u32(out, (uint32_t)msg.size());
    buf_append(out, (const uint8_t *)msg.data(), msg.size());
}

void out_arr(Buffer &out, uint32_t n)
{
    buf_append_u8(out, TAG_ARR);
    buf_append_u32(out, n);
}
