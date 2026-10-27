#include "InAgent.h"

int agent_connect(agent_t *a) {
    agent_cleanup(a);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "socket: %s", strerror(errno));
        return ERR;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)a->port);

    if (inet_pton(AF_INET, a->host, &addr.sin_addr) <= 0) {
        syslog(LOG_ERR, "inet_pton(%s) failed", a->host);
        close(fd);
        return ERR;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "connect to %s:%d: %s", a->host, a->port, strerror(errno));
        close(fd);
        return ERR;
    }

    a->sock = fd;
    syslog(LOG_INFO, "Connected to %s:%d", a->host, a->port);
    return OK;
}

int agent_register(agent_t *a) {
    if (a->sock < 0) return ERR;

    json_t *obj = json_object();
    if (!obj) return ERR;

    json_object_set_new(obj, "type", json_string("Agent_Register"));
    json_object_set_new(obj, "device_id", json_string(a->device_id));
    json_object_set_new(obj, "type", json_string(a->type));
    json_object_set_new(obj, "firmware", json_string(a->firmware));

    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    if (!str) return ERR;

    int rc = send_frame(a->sock, str, strlen(str));
    free(str);
    if (rc != OK) return ERR;

    char *resp = recv_frame(a->sock);
    if (!resp) {
        syslog(LOG_ERR, "No Register_ack received");
        return ERR;
    }

    json_error_t err;
    json_t *root = json_loads(resp, 0, &err);
    free(resp);
    if (!root) {
        syslog(LOG_ERR, "Register_ack parse error: %s", err.text);
        return ERR;
    }

    json_t *jtype   = json_object_get(root, "type");
    json_t *jstatus = json_object_get(root, "status");
    const char *status = json_is_string(jstatus) ? json_string_value(jstatus) : "";

    int ok = 0;
    if (json_is_string(jtype) &&
        strcmp(json_string_value(jtype), "Register_ack") == 0 &&
        (strcmp(status, "SUCCESS") == 0 ||
         strcmp(status, "ok") == 0 ||
         strcmp(status, "OK") == 0)) {
        ok = 1;
        a->registered = 1;
        syslog(LOG_INFO, "Registered successfully as %s", a->device_id);
    } else {
        const char *reason = "unknown";
        json_t *jreason = json_object_get(root, "reason");
        if (json_is_string(jreason))
            reason = json_string_value(jreason);
        syslog(LOG_ERR, "Registration failed: status=%s reason=%s", status, reason);
    }

    json_decref(root);
    return ok ? OK : ERR;
}