#include "InServer.h"

static int frame(int fd, const char *body, size_t body_len){
    uint32_t len = htonl(body_len);
    if(write(fd, &len, sizeof(len)) != sizeof(len)){
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
    if(operator_fd < 0 || device_id == NULL || kind == NULL || details == NULL){return ERR;}

    json_t *obj = json_object();
    if(!obj){return ERR;}

    json_object_set_new(obj, "type", json_string("Alert"));
    json_object_set_new(obj, "device_id", json_string(device_id));
    json_object_set_new(obj, "kind", json_string(kind));
    json_object_set(obj, "ts", json_integer(time(NULL)));

    if(details && json_is_object(details)) {
        json_object_set(obj, "details", details);
    } else {
        json_object_set_new(obj, "details", json_object());
    }

    const char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    if(!str){return ERR;}

    int result = frame(operator_fd, str, strlen(str));
    free(str);

    if(result == OK){
        syslog(LOG_INFO, "Alert отправлен оператору: device_id=%s, kind=%s\n", device_id, kind);
    } else {
        syslog(LOG_ERR, "Ошибка отправки Alert оператору: device_id=%s, kind=%s\n", device_id, kind);
    }
    return result;
}

int Alert_broadcast(MAX_EVENTS_t *ev, const char *device_id, const char *kind, json_t *details){
    // тут не забыть потом доделать отправку всем операторам, кроме того, кто отправил
}