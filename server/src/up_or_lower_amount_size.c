#include "InServer.h"

int add_connection(MAX_EVENTS_t *ev){
    ev->amount_connections++;
    return OK;
}
int remove_connection(MAX_EVENTS_t *ev){
    if(ev->amount_connections > 0){ev->amount_connections--;}
    return OK;
}

int UP_amount_size(MAX_EVENTS_t *ev){
    if(ev->amount_connections >= ev->amount_size){
        size_t new_size = ev->amount_size * 2;
        struct epoll_event *tmp = realloc(ev->events, new_size * sizeof(struct epoll_event));

        if(tmp == NULL){return ERR;}
        ev->events = tmp;
        ev->amount_size = new_size;
    }
    return add_connection(ev);
}
int REDUCE_amount_size(MAX_EVENTS_t *ev){
    remove_connection(ev);
    if(ev->amount_connections <= ev->amount_size/4 && ev->amount_size > MIN_SIZE){
        size_t new_size = ev->amount_size/2;
        struct epoll_event *tmp = realloc(ev->events, new_size * sizeof(struct epoll_event));

        if(tmp != NULL){
            ev->events = tmp;
            ev->amount_size = new_size;
        }
    }
    return OK;
}