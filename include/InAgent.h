#ifndef INAGENT_H
#define INAGENT_H

#include "library.h"

#define DEFAULT_SERVER_HOST "127.0.0.1"
#define DEFAULT_SERVER_PORT 9000
#define DEFAULT_INTERVAL 10
#define DEFAULT_DEVICE_ID "sw-01"
#define DEFAULT_TYPE "switch"
#define DEFAULT_FIRMWARE "1.0.0"
#define MAX_PORTS 8
#define HEARTBEAT_INTERVAL 5

typedef struct {
    int id;
    char state[8];
    long long rx;
    long long tx;
} port_t;

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


int   send_frame(int fd, const char *body, size_t body_len);
char *recv_frame(int fd);
void agent_init(agent_t *a);
void agent_cleanup(agent_t *a);
int agent_connect(agent_t *a);
int agent_register(agent_t *a);
void agent_update_metrics(agent_t *a);
int  agent_send_metrics(agent_t *a);
int agent_send_heartbeat(agent_t *a);
int agent_handle_command(agent_t *a, json_t *root);
int agent_process_incoming(agent_t *a);

#endif