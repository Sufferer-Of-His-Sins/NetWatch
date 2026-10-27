#include "InAgent.h"

int agent_handle_command(agent_t *a, json_t *root) {
    json_t *jcmd = json_object_get(root, "command");
    if (!json_is_string(jcmd)) {
        syslog(LOG_ERR, "Command without 'command' field");
        return ERR;
    }

    const char *cmd = json_string_value(jcmd);
    syslog(LOG_INFO, "Received command: %s", cmd);

    json_t *resp = json_object();
    if (!resp) return ERR;

    json_object_set_new(resp, "type", json_string("Command_result"));
    json_object_set_new(resp, "device_id", json_string(a->device_id));
    json_object_set_new(resp, "command", json_string(cmd));

    int ok = 1;
    const char *reason = "OK";

    if (strcmp(cmd, "set_interval") == 0) {
        json_t *jint = json_object_get(root, "interval");
        if (!json_is_integer(jint)) {
            ok = 0;
            reason = "Missing or invalid interval";
        } else {
            int iv = (int)json_integer_value(jint);
            if (iv < 1 || iv > 3600) {
                ok = 0;
                reason = "Interval out of range (1-3600)";
            } else {
                a->interval = iv;
                syslog(LOG_INFO, "Interval set to %d sec", iv);
                reason = "Interval updated";
            }
        }
    } else if (strcmp(cmd, "reboot") == 0) {
        syslog(LOG_INFO, "Reboot requested — will reconnect");
        a->need_reboot = 1;
        reason = "Rebooting";
    } else if (strcmp(cmd, "get_status") == 0) {
        json_t *data = json_object();
        json_object_set_new(data, "device_id", json_string(a->device_id));
        json_object_set_new(data, "type", json_string(a->type));
        json_object_set_new(data, "firmware", json_string(a->firmware));
        json_object_set_new(data, "cpu", json_real(a->cpu));
        json_object_set_new(data, "ram", json_real(a->ram));
        json_object_set_new(data, "uptime",
                            json_integer((json_int_t)(time(NULL) - a->uptime_start)));
        json_object_set_new(data, "interval", json_integer(a->interval));
        json_object_set_new(data, "online", json_true());

        json_t *ports = json_array();
        for (int i = 0; i < a->num_ports; i++) {
            json_t *p = json_object();
            json_object_set_new(p, "id", json_integer(a->ports[i].id));
            json_object_set_new(p, "state", json_string(a->ports[i].state));
            json_object_set_new(p, "rx", json_integer(a->ports[i].rx));
            json_object_set_new(p, "tx", json_integer(a->ports[i].tx));
            json_array_append_new(ports, p);
        }
        json_object_set_new(data, "ports", ports);
        json_object_set_new(resp, "data", data);
        reason = "Status retrieved";
    } else {
        ok = 0;
        reason = "Unknown command";
    }

    json_object_set_new(resp, "status", json_string(ok ? "SUCCESS" : "ERROR"));
    json_object_set_new(resp, "reason", json_string(reason));

    char *str = json_dumps(resp, JSON_COMPACT);
    json_decref(resp);
    if (!str) return ERR;

    int rc = send_frame(a->sock, str, strlen(str));
    free(str);
    return rc;
}

int agent_process_incoming(agent_t *a) {
    char *body = recv_frame(a->sock);
    if (!body) return ERR;

    json_error_t err;
    json_t *root = json_loads(body, 0, &err);
    free(body);
    if (!root) {
        syslog(LOG_ERR, "Incoming JSON parse error: %s", err.text);
        return OK;
    }

    json_t *jtype = json_object_get(root, "type");
    if (!json_is_string(jtype)) {
        json_decref(root);
        return OK;
    }

    const char *type = json_string_value(jtype);

    if (strcmp(type, "Command") == 0) {
        agent_handle_command(a, root);
    } else if (strcmp(type, "Heartbeat_ack") == 0) {
        syslog(LOG_DEBUG, "Heartbeat_ack received");
    } else if (strcmp(type, "Register_ack") == 0) {
        syslog(LOG_DEBUG, "Late Register_ack ignored");
    } else {
        syslog(LOG_INFO, "Unhandled message type: %s", type);
    }

    json_decref(root);
    return OK;
}