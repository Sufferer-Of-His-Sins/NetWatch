#ifndef LIBRARY_H
#define LIBRARY_H

#include <time.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <syslog.h>
#include <jansson.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <postgresql/libpq-fe.h>

#define MIN_SIZE 8
#define BUFFER 1024
#define HISTORY_LIMIT 100
#define MAX_FRAME_HISTORY (1024 * 1024)
#define DEFAULT_SERVER_HOST "127.0.0.1"
#define DEFAULT_SERVER_PORT 9000
#define DEFAULT_INTERVAL 10
#define DEFAULT_DEVICE_ID "sw-01"
#define DEFAULT_TYPE "switch"
#define DEFAULT_FIRMWARE "1.0.0"
#define MAX_PORTS 8
#define MAX_CONNS 256
#define HEARTBEAT_INTERVAL 5
#define HEARTBEAT_TIMEOUT 30


typedef enum {
    OK = 0,
    ERR = -1,
    BUF = 1024,
} status_t;

typedef struct {
    char device_id[64];
    char type[32];
    char firmware[32];
    char host[64];
    int port;
    int interval;
    int sock;
    int registered;
    int running;

    double cpu;
    double ram;
    long long uptime_start;
    port_t ports[MAX_PORTS];
    int num_ports;
    int need_reboot;
} agent_t;

typedef struct {
    int id;
    char state[8];
    long long rx;
    long long tx;
} port_t;

typedef struct{
    int amount_size;
    int amount_connections;
    struct epoll_event *events;
} MAX_EVENTS_t;

typedef struct {
    int fd;
    int is_agent;
    char device_id[64];
    time_t last_seen;
    char *recv_buf;
    size_t recv_len;
    size_t recv_cap;
} conn_t;
#endif