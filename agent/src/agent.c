#include "InAgent.h"

void agent_init(agent_t *a) {
    memset(a, 0, sizeof(*a));

    strncpy(a->device_id, DEFAULT_DEVICE_ID, sizeof(a->device_id) - 1);
    strncpy(a->type, DEFAULT_TYPE, sizeof(a->type) - 1);
    strncpy(a->firmware, DEFAULT_FIRMWARE, sizeof(a->firmware) - 1);
    strncpy(a->host, DEFAULT_SERVER_HOST, sizeof(a->host) - 1);

    a->port = DEFAULT_SERVER_PORT;
    a->interval = DEFAULT_INTERVAL;
    a->sock = -1;
    a->registered = 0;
    a->running = 1;
    a->need_reboot = 0;

    a->cpu = 30.0 + (rand() % 20);
    a->ram = 40.0 + (rand() % 20);
    a->uptime_start = time(NULL);

    a->num_ports = 4 + (rand() % 5);
    if (a->num_ports > MAX_PORTS)
        a->num_ports = MAX_PORTS;

    for (int i = 0; i < a->num_ports; i++) {
        a->ports[i].id = i + 1;
        strcpy(a->ports[i].state, (rand() % 10) ? "up" : "down");
        a->ports[i].rx = (long long)(rand() % 10000);
        a->ports[i].tx = (long long)(rand() % 8000);
    }
}

void agent_cleanup(agent_t *a) {
    if (a->sock >= 0) {
        close(a->sock);
        a->sock = -1;
    }
    a->registered = 0;
}