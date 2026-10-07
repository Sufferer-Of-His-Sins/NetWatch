#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <jansson.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <postgresql/libpq-fe.h>


typedef enum {
    OK = 0,
    ERR = -1,
    BUF = 1024,
} status_t;

#endif