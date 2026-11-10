#include "InOperator.h"

int send_frame(int fd, const char *body, size_t body_len) {
    if (fd < 0 || !body) return ERR;
    uint32_t net_len = htonl((uint32_t)body_len);
    if (write(fd, &net_len, sizeof(net_len)) != (ssize_t)sizeof(net_len)) return ERR;
    if (write(fd, body, body_len) != (ssize_t)body_len) return ERR;
    return OK;
}

static int read_all(int fd, void *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = read(fd, (char *)buf + got, len - got);
        if (n == 0) return ERR;
        if (n < 0) {
            if (errno == EINTR) continue;
            return ERR;
        }
        got += (size_t)n;
    }
    return OK;
}

char *recv_frame(int fd) {
    uint32_t net_len;
    if (read_all(fd, &net_len, sizeof(net_len)) != OK) return NULL;
    uint32_t len = ntohl(net_len);
    if (len == 0 || len > 65536) return NULL;
    char *body = malloc(len + 1);
    if (!body) return NULL;
    if (read_all(fd, body, len) != OK) { free(body); return NULL; }
    body[len] = '\0';
    return body;
}

int operator_connect(const char *host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) { close(fd); return -1; }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    return fd;
}