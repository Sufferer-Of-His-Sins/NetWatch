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

/*отправка оператору*/
static int send_query_result(int operator_fd, const char *query, const char *status, const char *reason ,json_t *data){
    json_t *obj = json_object();
    if(!obj){return ERR;}

    json_object_set_new(obj, "type", json_string("Query_result"));
    json_object_set_new(obj, "query", json_string(query));
    json_object_set_new(obj, "status", json_string(status));
    
    if(reason){json_object_set_new(obj, "reason", json_string(reason));}
    if(data){json_object_set(obj, "data", data);}
    else {json_object_set_new(obj, "data", json_null());}
    
    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    if(!str){return ERR;}

    int result = frame(operator_fd, str, strlen(str));
    free(str);
    return result;
}
/*журнал тревог*/
static int log_alerts(int operator_fd){
    json_t *alerts = json_array();
    if(!alerts){return ERR;}

    int result = send_query_result(operator_fd, "alerts(journal)", "ok", NULL, alerts);
    json_decref(alerts);
    return result;
}
/*история метрик*/
static int log_metrics(int operator_fd, const char *device_id, json_int_t from_ts, json_int_t ts){
    if(!device_id){
        return send_query_result(operator_fd, "metrics_history(logs)", "error", "Missing_device_id", NULL);
    }

    json_t *metrics = json_array();
    if(!metrics){return ERR;}

    int result = send_query_result(operator_fd, "metrics_history(logs)", "ok", NULL, metrics);
    return result;
}
/*подробнее об устройстве*/
static int device_details(int operator_fd, const char *device_id){
    if (!device_id) {
        return send_query_result(operator_fd, "device_details","error", "Missing device_id", NULL);
    }

    json_t *data = json_object();
    if (!data) return ERR;

    json_object_set_new(data, "device_id", json_string(device_id));
    json_object_set_new(data, "cpu", json_null());
    json_object_set_new(data, "ram", json_null());
    json_object_set_new(data, "uptime", json_null());
    json_object_set_new(data, "ports", json_array());
    json_object_set_new(data, "ts", json_null());

    int result = send_query_result(operator_fd, "device_details", "ok", NULL, data);
    json_decref(data);
    return result;
}
/*список наших устройств*/
static int list_devices(int operator_fd){
    json_t *data = json_array();
    if (!data) return ERR;

    json_t *dev = json_object();
    json_object_set_new(dev, "device_id", json_string("sw-014"));
    json_object_set_new(dev, "type", json_string("switch"));
    json_object_set_new(dev, "firmware", json_string("1.0.0"));
    json_object_set_new(dev, "online", json_true());
    json_object_set_new(dev, "last_seen", json_integer(time(NULL)));
    json_array_append_new(data, dev);
    

    int result = send_query_result(operator_fd, "list_devices", "ok", NULL, data);
    json_decref(data);
    return result;
}

/*основной цикл*/
int Query_result(const char *buffer, MAX_EVENTS_t *ev, int operator_fd){
    json_error_t error;
    json_t *root = json_loads(buffer, 0, &error);
    
    if(!root){
        syslog(LOG_ERR, "Ошибка парсинга строка %d: %s\n", error.line, error.text);
        return ERR;
    }
    json_t *type = json_object_get(root, "type");

    if(json_is_string(type) && strcmp(json_string_value(type), "Query") != 0){
        syslog(LOG_ERR, "Неверный тип сообщения: %s\n", json_string_value(type));
        json_decref(root);
        send_query_result(operator_fd, "", "error", "Missing type", NULL);
        return ERR;
    }

    json_t *query = json_object_get(root,"query");
    if (!json_is_string(query)){
        syslog(LOG_ERR,"Неверный тип сообщения: %s\n", json_string_value(query));
        json_decref(root);
        send_query_result(operator_fd, "", "error", "Missing query", NULL);
        return ERR;
    }
    
    int result = ERR;
    
    if(strcmp(json_string_value(query), "list_device") == 0){
        result = list_devices(operator_fd);
    } else if(strcmp(jsont_string_value(query), "device_details") == 0){
        json_t *device  = json_object_get(root, "device_id");
        const char *device_ID = json_string_value(device) ? json_is_string(device) : NULL;
        result = device_details(operator_fd, device_ID);
    } else if (strcmp(json_string_value(query), "alerts") == 0){
        result = log_alerts(operator_fd);
    } else if (strcmp(json_string_value(query), "metrics_history") == 0){
        json_t *j_dev  = json_object_get(root, "device_id");
        json_t *j_from = json_object_get(root, "from");
        json_t *j_to   = json_object_get(root, "to");

        const char *device_id = json_is_string(j_dev) ? json_string_value(j_dev) : NULL;
        json_int_t from_ts = json_is_integer(j_from) ? json_integer_value(j_from) : 0;
        json_int_t to_ts   = json_is_integer(j_to)? json_integer_value(j_to) : (json_int_t)time(NULL);

        result = log_metrics(operator_fd, device_id, from_ts, to_ts);
    } else {
        syslog(LOG_ERR, "Неизвестный аргумент в query");
        result = send_query_result(operator_fd, json_string_value(query),"error","Неизвестный тип query", NULL);
    }
    json_decref(root);
    return OK;
}