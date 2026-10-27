#include "InAgent.h"

int send_frame(int fd, const char *body, size_t body_len) {
    if (fd < 0 || !body) return ERR;

    uint32_t net_len = htonl((uint32_t)body_len);
    if (write(fd, &net_len, sizeof(net_len)) != (ssize_t)sizeof(net_len)) {
        syslog(LOG_ERR, "send_frame: length write failed: %s", strerror(errno));
        return ERR;
    }
    if (write(fd, body, body_len) != (ssize_t)body_len) {
        syslog(LOG_ERR, "send_frame: body write failed: %s", strerror(errno));
        return ERR;
    }
    return OK;
}

static int read_all(int fd, void *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = read(fd, (char *)buf + got, len - got);
        if (n == 0) {
            syslog(LOG_ERR, "read_all: peer closed connection");
            return ERR;
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return ERR;
            syslog(LOG_ERR, "read_all: %s", strerror(errno));
            return ERR;
        }
        got += (size_t)n;
    }
    return OK;
}

char *recv_frame(int fd) {
    uint32_t net_len;
    if (read_all(fd, &net_len, sizeof(net_len)) != OK)
        return NULL;

    uint32_t len = ntohl(net_len);
    if (len == 0 || len > 65536) {
        syslog(LOG_ERR, "recv_frame: invalid length %u", len);
        return NULL;
    }

    char *body = malloc(len + 1);
    if (!body) return NULL;

    if (read_all(fd, body, len) != OK) {
        free(body);
        return NULL;
    }
    body[len] = '\0';
    return body;
}