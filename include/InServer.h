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
int Register_ack(void);
int Heartbeat_ack(void);
int Command(void);
int Alert(void);
int Query_result(void);


/*Дополнительные функции*/
int add_connection(MAX_EVENTS_t *ev);
int remove_connection(MAX_EVENTS_t *ev);
int UP_amount_size(MAX_EVENTS_t *ev);
int REDUCE_amount_size(MAX_EVENTS_t *ev);
#endif