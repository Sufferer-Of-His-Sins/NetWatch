#ifndef INSERVER_H
#define INSERVER_H

#include "library.h"

#define MIN_SIZE 8

typedef struct{
    int amount_size;
    int amount_connections;
    struct epoll_event *events;
} MAX_EVENTS_t;


/*Основные функции сервера*/
int Server(void);
int Register_ack(const char *buffer, MAX_EVENTS_t *ev, int fd);
int Heartbeat_ack(int socket_fd);
int Command(const char *buffer, MAX_EVENTS_t *ev, int operator_fd);
int Alert(int socket_fd);
int Query_result(const char *buffer, MAX_EVENTS_t *ev, int fd);

/*Дополнительные функции*/
int add_connection(MAX_EVENTS_t *ev);
int remove_connection(MAX_EVENTS_t *ev);
int UP_amount_size(MAX_EVENTS_t *ev);
int REDUCE_amount_size(MAX_EVENTS_t *ev);
#endif