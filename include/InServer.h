#ifndef INSERVER_H
#define INSERVER_H

#include "library.h"



typedef struct{
    int amount_size;
    int amount_connections;
    struct epoll_event *events;
} MAX_EVENTS_t;


/*Основные функции сервера*/
int Server(void);
int Register_ack(const char *buffer, MAX_EVENTS_t *ev, int agent_fd);
int Heartbeat_ack(const char *buffer, MAX_EVENTS_t *ev, int agent_fd);
int Command(const char *buffer, MAX_EVENTS_t *ev, int operator_fd);
int Alert_send(int operator_fd, const char *device_id, const char *kind, json_t *details);
int Alert_broadcast(MAX_EVENTS_t *ev, const char *device_id, const char *kind, json_t *details);
int Query_result(const char *buffer, MAX_EVENTS_t *ev, int operator_fd);

/*Дополнительные функции*/
int add_connection(MAX_EVENTS_t *ev);
int remove_connection(MAX_EVENTS_t *ev);
int UP_amount_size(MAX_EVENTS_t *ev);
int REDUCE_amount_size(MAX_EVENTS_t *ev);
#endif