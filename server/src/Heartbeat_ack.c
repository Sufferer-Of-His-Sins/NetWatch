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

int Heartbeat_ack(const char *buffer, MAX_EVENTS_t *ev, int agent_fd){
    json_error_t error;
    json_t *root = json_loads(buffer, 0, &error);

    if(!root){
        syslog(LOG_ERR, "Ошибка парсинга строка %d: %s\n", error.line, error.text);
        return ERR;
    }

    json_t *type = json_object_get(root, "type");
    if(json_is_string(type) && strcmp(json_string_value(type), "Heartbeat") != 0){
        syslog(LOG_ERR, "Неверный тип сообщения: %s\n", json_string_value(type));
        json_decref(root);
        return ERR;
    }

    json_t *dev_id = json_object_get(root, "device_id");
    const char *device_id = json_string_value(dev_id);

    json_t *ack = json_object();
    if(!ack) {return ERR;}

    json_object_set_new(ack, "type", json_string("Heartbeat_ack"));
    json_object_set_new(ack, "device_id", json_string("ok"));

    char *str = json_dumps(ack, JSON_COMPACT);
    if(!str) {return ERR;}

    int result = frame(agent_fd, str, strlen(str));
    free(str);

    if(result == OK){
        syslog(LOG_INFO, "Отправлено Heartbeat_ack для устройства: %s\n", device_id);
    } else {
        syslog(LOG_ERR, "Ошибка отправки Heartbeat_ack для устройства: %s\n", device_id);
    }
}