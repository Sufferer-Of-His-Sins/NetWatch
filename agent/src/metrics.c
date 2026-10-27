#include "InAgent.h"


static double clamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static double rand_gauss(void) {
    double u1 = (rand() + 1.0) / (RAND_MAX + 1.0);
    double u2 = (rand() + 1.0) / (RAND_MAX + 1.0);
    return sqrt(-2.0 * log(u1)) * cos(2.0 * 3.141592653589793 * u2);
}

void agent_update_metrics(agent_t *a) {
    a->cpu += rand_gauss() * 1.5;
    a->cpu = clamp(a->cpu, 5.0, 95.0);
    if ((rand() % 200) == 0)
        a->cpu = clamp(a->cpu + 25.0 + (rand() % 20), 0, 99.0);

    a->ram += rand_gauss() * 0.8;
    a->ram = clamp(a->ram, 15.0, 90.0);
    if ((rand() % 300) == 0)
        a->ram = clamp(a->ram + 15.0, 0, 95.0);

    for (int i = 0; i < a->num_ports; i++) {
        port_t *p = &a->ports[i];
        if ((rand() % 500) == 0)
            strcpy(p->state, (strcmp(p->state, "up") == 0) ? "down" : "up");

        if (strcmp(p->state, "up") == 0) {
            p->rx += 50 + (rand() % 400);
            p->tx += 30 + (rand() % 300);
        }
    }
}

int agent_send_metrics(agent_t *a) {
    if (a->sock < 0 || !a->registered) return ERR;

    agent_update_metrics(a);

    json_t *obj = json_object();
    if (!obj) return ERR;

    json_object_set_new(obj, "type", json_string("Metrics"));
    json_object_set_new(obj, "device_id", json_string(a->device_id));
    json_object_set_new(obj, "ts", json_integer((json_int_t)time(NULL)));
    json_object_set_new(obj, "cpu", json_real(a->cpu));
    json_object_set_new(obj, "ram", json_real(a->ram));
    json_object_set_new(obj, "uptime",
                        json_integer((json_int_t)(time(NULL) - a->uptime_start)));

    json_t *ports = json_array();
    for (int i = 0; i < a->num_ports; i++) {
        json_t *p = json_object();
        json_object_set_new(p, "id", json_integer(a->ports[i].id));
        json_object_set_new(p, "state", json_string(a->ports[i].state));
        json_object_set_new(p, "rx", json_integer(a->ports[i].rx));
        json_object_set_new(p, "tx", json_integer(a->ports[i].tx));
        json_array_append_new(ports, p);
    }
    json_object_set_new(obj, "ports", ports);

    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    if (!str) return ERR;

    int rc = send_frame(a->sock, str, strlen(str));
    free(str);

    if (rc == OK)
        syslog(LOG_DEBUG, "Metrics sent: cpu=%.1f ram=%.1f", a->cpu, a->ram);
    return rc;
}