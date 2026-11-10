#ifndef INSERVER_H
#define INSERVER_H

#include "library.h"

/*Основные функции сервера*/
int   Server(void);
int   Register_ack(const char *buffer, MAX_EVENTS_t *ev, int agent_fd);
int   Heartbeat_ack(const char *buffer, MAX_EVENTS_t *ev, int agent_fd);
int   Command(const char *buffer, MAX_EVENTS_t *ev, int operator_fd);
int   Alert_send(int operator_fd, const char *device_id, const char *kind, json_t *details);
int   Alert_broadcast(MAX_EVENTS_t *ev, const char *device_id, const char *kind, json_t *details);
int   Query_result(const char *buffer, MAX_EVENTS_t *ev, int operator_fd);
int   find_agent_fd(const char *device_id);

conn_t *server_get_conns(int *n);

/*Дополнительные функции*/
int add_connection(MAX_EVENTS_t *ev);
int remove_connection(MAX_EVENTS_t *ev);
int UP_amount_size(MAX_EVENTS_t *ev);
int REDUCE_amount_size(MAX_EVENTS_t *ev);

/*postgre*/
int db_connect(void);
void db_disconnect(void);
int db_ensure(void);
int db_upsert_device(const char *device_id, const char *type, 
                        const char *firmware, int online, time_t last_seen);
int db_insert_metrics(const char *device_id, time_t ts,
                        double cpu, double ram, long long uptime, json_t *ports);
int db_insert_alert(const char *device_id, const char *kind, json_t *details);
json_t *db_list_devices(void);
json_t *db_device_details(const char *device_id);
json_t *db_metrics_history(const char *device_id, time_t from, time_t to);
json_t *db_alerts(void);
#endif