#include "InServer.h"

static volatile sig_atomic_t g_running = 1;
static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {return ERR;}
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
static conn_t conns[MAX_CONNS];
static int nconns = 0;
static conn_t *find_conn(int fd) {
    for (int i = 0; i < nconns; i++) {
        if (conns[i].fd == fd) {return &conns[i];}
    }
    return NULL;
}
static conn_t *add_conn(int fd) {
    if (nconns >= MAX_CONNS) {return NULL;}
    conn_t *c = &conns[nconns++];
    memset(c, 0, sizeof(*c));
    c->fd = fd;
    c->is_agent = -1;
    c->last_seen = time(NULL);
    c->recv_cap = 4096;
    c->recv_buf = malloc(c->recv_cap);
    if (!c->recv_buf) {
        nconns--;
        return NULL;
    }
    c->recv_len = 0;
    return c;
}
static void remove_conn(int fd) {
    for (int i = 0; i < nconns; i++) {
        if (conns[i].fd == fd) {
            free(conns[i].recv_buf);
            close(fd);
            conns[i] = conns[--nconns];
            return;
        }
    }
}
static char *recv_frame(conn_t *c) {
    while (1) {
        if (c->recv_len < 4) {
            ssize_t n = recv(c->fd, c->recv_buf + c->recv_len, c->recv_cap - c->recv_len, 0);
            if (n <= 0) {
                if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {return NULL;}
                return (char *)-1;
            }
            c->recv_len += (size_t)n;
            continue;
        }

        uint32_t net_len;
        memcpy(&net_len, c->recv_buf, 4);
        uint32_t body_len = ntohl(net_len);

        if (body_len > MAX_FRAME_HISTORY || body_len == 0) {
            syslog(LOG_ERR, "Invalid frame length %u on fd=%d\n", body_len, c->fd);
            return NULL;
        }

        size_t total = 4 + body_len;
        if (c->recv_cap < total) {
            char *nb = realloc(c->recv_buf, total + 1024);
            if (!nb) return NULL;
            c->recv_buf = nb;
            c->recv_cap = total + 1024;
        }

        if (c->recv_len < total) {
            ssize_t n = recv(c->fd, c->recv_buf + c->recv_len, total - c->recv_len, 0);
            if (n <= 0) {
                if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                    return NULL;
                }
                return (char *)-1;
            }
            c->recv_len += (size_t)n;
            continue;
        }

        char *body = malloc(body_len + 1);
        if (!body) {return NULL;}

        memcpy(body, c->recv_buf + 4, body_len);
        body[body_len] = '\0';

        size_t remaining = c->recv_len - total;
        if (remaining > 0) {memmove(c->recv_buf, c->recv_buf + total, remaining);}
        c->recv_len = remaining;

        return body;
    }
}

static void handle_message(conn_t *c, char *body, MAX_EVENTS_t *ev) {
    json_error_t error;
    json_t *root = json_loads(body, 0, &error);
    if (!root) {
        syslog(LOG_ERR, "JSON parse error: %s\n", error.text);
        free(body);
        return;
    }

    json_t *jtype = json_object_get(root, "type");
    const char *type = json_is_string(jtype) ? json_string_value(jtype) : NULL;

    if (!type) {
        json_decref(root);
        free(body);
        return;
    }

    c->last_seen = time(NULL);

    if (strcmp(type, "Agent_Register") == 0) {
        c->is_agent = 1;
        json_t *jid = json_object_get(root, "device_id");
        if (json_is_string(jid)) {strncpy(c->device_id, json_string_value(jid), sizeof(c->device_id) - 1);}
        Register_ack(body, ev, c->fd);
    } else if (strcmp(type, "Heartbeat") == 0) {
        c->is_agent = 1;
        json_t *jid = json_object_get(root, "device_id");
        if (json_is_string(jid)) {strncpy(c->device_id, json_string_value(jid), sizeof(c->device_id) - 1);}
        Heartbeat_ack(body, ev, c->fd);
    } else if (strcmp(type, "metrics") == 0 || strcmp(type, "Metrics") == 0) {
        c->is_agent = 1;
        json_t *jid = json_object_get(root, "device_id");
        if (json_is_string(jid)) {strncpy(c->device_id, json_string_value(jid), sizeof(c->device_id) - 1);}
        syslog(LOG_INFO, "Metrics from %s\n", c->device_id);
    } else if (strcmp(type, "Command_request") == 0) {
        c->is_agent = 0;
        Command(body, ev, c->fd);
    } else if (strcmp(type, "Query") == 0) {
        c->is_agent = 0;
        Query_result(body, ev, c->fd);
    } else if (strcmp(type, "Command_result") == 0) {
        syslog(LOG_INFO, "Command_result received\n");
    } else {
        syslog(LOG_WARNING, "Unknown message type: %s\n", type);
    }

    json_decref(root);
    free(body);
}

int find_agent_fd(const char *device_id) {
    if (!device_id) return -1;
    for (int i = 0; i < nconns; i++) {
        if (conns[i].is_agent == 1 && strcmp(conns[i].device_id, device_id) == 0) {return conns[i].fd;}
    }
    return -1;
}

int Server(void) {
    openlog("netwatch-server", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "NetWatch server starting...\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        syslog(LOG_ERR, "socket: %s\n", strerror(errno));
        return ERR;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DEFAULT_SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "bind: %s\n", strerror(errno));
        close(listen_fd);
        return ERR;
    }

    if (listen(listen_fd, 32) < 0) {
        syslog(LOG_ERR, "listen: %s\n", strerror(errno));
        close(listen_fd);
        return ERR;
    }

    set_nonblocking(listen_fd);

    int epfd = epoll_create1(0);
    if (epfd < 0) {
        syslog(LOG_ERR, "epoll_create1: %s\n", strerror(errno));
        close(listen_fd);
        return ERR;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        syslog(LOG_ERR, "epoll_ctl listen: %s\n", strerror(errno));
        close(epfd);
        close(listen_fd);
        return ERR;
    }

    MAX_EVENTS_t maxev = {0};
    maxev.amount_size = MIN_SIZE;
    maxev.amount_connections = 0;
    maxev.events = calloc(MIN_SIZE, sizeof(struct epoll_event));
    if (!maxev.events) {
        close(epfd);
        close(listen_fd);
        return ERR;
    }

    struct epoll_event events[MIN_SIZE];
    time_t last_check = time(NULL);

    while (g_running) {
        int nfds = epoll_wait(epfd, events, MIN_SIZE, 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "epoll_wait: %s\n", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                struct sockaddr_in cli;
                socklen_t clilen = sizeof(cli);
                int client = accept(listen_fd, (struct sockaddr *)&cli, &clilen);
                if (client < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK){syslog(LOG_ERR, "accept: %s\n", strerror(errno));}
                    continue;
                }
                set_nonblocking(client);
                struct epoll_event cev;
                cev.events = EPOLLIN | EPOLLET;
                cev.data.fd = client;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client, &cev) < 0) {
                    syslog(LOG_ERR, "epoll_ctl add: %s\n", strerror(errno));
                    close(client);
                    continue;
                }
                if (!add_conn(client)) {
                    syslog(LOG_ERR, "Too many connections\n");
                    close(client);
                    continue;
                }
                UP_amount_size(&maxev);
                syslog(LOG_INFO, "New connection fd=%d from %s\n", client, inet_ntoa(cli.sin_addr));
            } else {
                conn_t *c = find_conn(fd);
                if (!c) {
                    close(fd);
                    continue;
                }

                while (1) {
                    char *body = recv_frame(c);
                    if (body == NULL) {
                        syslog(LOG_INFO, "Connection closed fd=%d device=%s\n", fd, c->device_id);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        remove_conn(fd);
                        REDUCE_amount_size(&maxev);
                        break;
                    }
                    if (body == (char *)-1) {
                        break;
                    }
                    handle_message(c, body, &maxev);
                }
            }
        }

        time_t now = time(NULL);
        if (now - last_check >= 5) {
            last_check = now;
            for (int i = 0; i < nconns; i++) {
                if (conns[i].is_agent == 1 && (now - conns[i].last_seen) > HEARTBEAT_TIMEOUT) {
                    syslog(LOG_WARNING, "Device %s timed out (last_seen %ld s ago)\n",conns[i].device_id, (long)(now - conns[i].last_seen));
                    
                    json_t *details = json_object();
                    json_object_set_new(details, "reason", json_string("heartbeat timeout"));
                    Alert_broadcast(&maxev, conns[i].device_id, "device_down", details);
                    json_decref(details);
                }
            }
        }
    }

    syslog(LOG_INFO, "Shutting down...\n");
    for (int i = 0; i < nconns; i++) {
        free(conns[i].recv_buf);
        close(conns[i].fd);
    }
    free(maxev.events);
    close(epfd);
    close(listen_fd);
    closelog();
    return OK;
}