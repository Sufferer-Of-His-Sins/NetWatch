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

typedef enum {
    OK = 0,
    ERR = -1,
    BUF = 1024,
} status_t;

#endif