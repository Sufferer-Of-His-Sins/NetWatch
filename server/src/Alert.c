#include "InServer.h"

extern conn_t *server_get_conns(int *n);

static int frame(int fd, const char *body, size_t body_len){
    uint32_t len = htonl(body_len);
    if(write(fd, &len, sizeof(len)) != (ssize_t)sizeof(len)){
        syslog(LOG_ERR, "Ошибка записи длины кадра: %s\n", strerror(errno));
        return ERR;
    }
    if(write(fd, body, body_len) != (ssize_t)body_len){
        syslog(LOG_ERR, "Ошибка записи тела кадра: %s\n", strerror(errno));
        return ERR;
    }
    return OK;
}

int Alert_send(int operator_fd, const char *device_id, const char *kind, json_t *details){
    if(operator_fd < 0 || !device_id || !kind) return ERR;

    json_t *obj = json_object();
    if(!obj) return ERR;

    json_object_set_new(obj, "type", json_string("Alert"));
    json_object_set_new(obj, "device_id", json_string(device_id));
    json_object_set_new(obj, "kind", json_string(kind));
    json_object_set_new(obj, "ts", json_integer((json_int_t)time(NULL)));
    if(details && json_is_object(details))
        json_object_set(obj, "details", details);
    else
        json_object_set_new(obj, "details", json_object());

    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    if(!str) return ERR;

    int result = frame(operator_fd, str, strlen(str));
    free(str);

    if(result == OK)
        syslog(LOG_INFO, "Alert отправлен оператору: device_id=%s, kind=%s\n", device_id, kind);
    else
        syslog(LOG_ERR, "Ошибка отправки Alert: device_id=%s, kind=%s\n", device_id, kind);
    return result;
}

int Alert_broadcast(MAX_EVENTS_t *ev, const char *device_id, const char *kind, json_t *details){
    (void)ev;
    db_insert_alert(device_id, kind, details);

    int n = 0;
    conn_t *conns = server_get_conns(&n);
    int sent = 0;
    for(int i = 0; i < n; i++){
        if(conns[i].is_agent == 0){ /* operator */
            if(Alert_send(conns[i].fd, device_id, kind, details) == OK)
                sent++;
        }
    }
    syslog(LOG_INFO, "Alert_broadcast: %s %s -> %d operators\n",
           device_id ? device_id : "?", kind ? kind : "?", sent);
    return OK;
}