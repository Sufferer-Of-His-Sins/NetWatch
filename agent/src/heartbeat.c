#include "InAgent.h"

int agent_send_heartbeat(agent_t *a) {
    if (a->sock < 0 || !a->registered) return ERR;

    json_t *obj = json_object();
    if (!obj) return ERR;

    json_object_set_new(obj, "type", json_string("Heartbeat"));
    json_object_set_new(obj, "device_id", json_string(a->device_id));

    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    if (!str) return ERR;

    int rc = send_frame(a->sock, str, strlen(str));
    free(str);

    if (rc == OK){syslog(LOG_DEBUG, "Heartbeat sent");}
    return rc;
}