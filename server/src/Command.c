#include "InServer.h"

static int send_framed(int fd, const char *str, size_t len){
    uint32_t net_len = htonl((uint32_t)len);
    ssize_t n = send(fd, &net_len, 4, 0);

    if (n != 4) {
        syslog(LOG_ERR, "Command: ошибка отправки длины: %s\n", strerror(errno));
        return ERR;
    }
    n = send(fd, str, len, 0);
    if (n != (ssize_t)len) {
        syslog(LOG_ERR, "Command: ошибка отправки тела: %s\n", strerror(errno));
        return ERR;
    }
    return OK;
}
static int reply_to_operator(int operator_fd, const char *status, const char *reason){
    json_t *obj = json_object();
    if (!obj) {return ERR;}

    json_object_set_new(obj, "type", json_string("Command_result"));
    json_object_set_new(obj, "status", json_string(status));
    json_object_set_new(obj, "reason", json_string(reason));

    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    if (!str) {return ERR;}

    int rc = send_framed(operator_fd, str, strlen(str));
    free(str);
    return rc;
}
/*чуть позже надо будет сделать*/
static int find_agent_fd(const char *device_id){
    return ERR;
}



int Command_send(int agent_fd, const char *command, json_t *params){
    if (agent_fd < 0 || command == NULL) {return ERR;}

    json_t *obj = json_object();
    if (!obj) {return ERR;}

    json_object_set_new(obj, "type", json_string("Command"));
    json_object_set_new(obj, "command", json_string(command));

    if (params && json_is_object(params)) {
        const char *key;
        json_t *value;
        json_object_foreach(params, key, value) {
            json_object_set(obj, key, value);
        }
    }

    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    if (!str) {return ERR;}

    int rc = send_framed(agent_fd, str, strlen(str));
    free(str);
    return rc;
}



int Command(const char *buffer, MAX_EVENTS_t *ev, int operator_fd){
    json_error_t error;
    json_t *root = json_loads(buffer, 0, &error);
 
    if (!root) {
        syslog(LOG_ERR, "Command: ошибка парсинга строка %d: %s\n", error.line, error.text);
        return ERR;
    }

    json_t *type = json_object_get(root, "type");
    if (!json_is_string(type) || strcmp(json_string_value(type), "Command_request") != 0){
        syslog(LOG_ERR, "ожидался type=Command_request\n");
        json_decref(root);
        reply_to_operator(operator_fd, "ERROR", "Invalid message type");
        return ERR;
    }


    json_t *j_device = json_object_get(root, "device_id");
    if (!json_is_string(j_device)) {
        syslog(LOG_ERR, "нет поля device_id\n");
        json_decref(root);
        reply_to_operator(operator_fd, "ERROR", "Missing device_id");
        return ERR;
    }
    const char *device_id = json_string_value(j_device);


    json_t *j_cmd = json_object_get(root, "command");
    if (!json_is_string(j_cmd)) {
        syslog(LOG_ERR, "нет поля command\n");
        json_decref(root);
        reply_to_operator(operator_fd, "ERROR", "Missing command");
        return ERR;
    }
    const char *command = json_string_value(j_cmd);


    /* Проверка допустимых команд + параметры */
    json_t *params = NULL;
    int need_free_params = 0;

    if (strcmp(command, "set_interval") == 0) {
        json_t *j_interval = json_object_get(root, "interval");
        if (!json_is_integer(j_interval)) {
            syslog(LOG_ERR, "set_interval без interval\n");
            json_decref(root);
            reply_to_operator(operator_fd, "ERROR", "Missing or invalid interval");
            return ERR;
        }
        int interval = (int)json_integer_value(j_interval);
        if (interval < 1 || interval > 3600) {
            syslog(LOG_ERR, "interval вне диапазона: %d\n", interval);
            json_decref(root);
            reply_to_operator(operator_fd, "ERROR", "Interval out of range (1-3600)");
            return ERR;
        }
        params = json_object();
        if (!params) {
            json_decref(root);
            return ERR;
        }
        json_object_set_new(params, "interval", json_integer(interval));
        need_free_params = 1;

    } else if (strcmp(command, "reboot") == 0 || strcmp(command, "get_status") == 0) {
        params = NULL;
    } else {
        syslog(LOG_ERR, "неизвестная команда '%s'\n", command);
        json_decref(root);
        reply_to_operator(operator_fd, "ERROR", "Unknown command");
        return ERR;
    }

    /* --- поиск агента --- */
    int agent_fd = find_agent_fd(device_id);
    if (agent_fd < 0) {
        syslog(LOG_ERR, "Command: устройство '%s' не найдено / offline\n", device_id);
        if (need_free_params) json_decref(params);
        json_decref(root);
        reply_to_operator(operator_fd, "ERROR", "Device not found or offline");
        return ERR;
    }

    /* --- пересылка агенту --- */
    int rc = Command_send(agent_fd, command, params);
    if (need_free_params) {json_decref(params);}
    json_decref(root);

    if (rc != OK) {
        reply_to_operator(operator_fd, "ERROR", "Failed to send command to agent");
        return ERR;
    }

    reply_to_operator(operator_fd, "SUCCESS", "Command sent to agent");
    syslog(LOG_INFO, "Command: '%s' отправлена устройству '%s'\n", command, device_id);
    return OK;
}