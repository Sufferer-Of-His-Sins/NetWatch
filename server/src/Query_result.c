#include "InServer.h"

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

static int send_query_result(int operator_fd, const char *query, const char *status,
                             const char *reason, json_t *data){
    json_t *obj = json_object();
    if(!obj) return ERR;

    json_object_set_new(obj, "type", json_string("Query_result"));
    json_object_set_new(obj, "query", json_string(query ? query : ""));
    json_object_set_new(obj, "status", json_string(status));
    if(reason) json_object_set_new(obj, "reason", json_string(reason));
    if(data) json_object_set(obj, "data", data);
    else json_object_set_new(obj, "data", json_null());

    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    if(!str) return ERR;

    int result = frame(operator_fd, str, strlen(str));
    free(str);
    return result;
}

static int log_alerts(int operator_fd){
    json_t *alerts = db_alerts();
    if(!alerts) alerts = json_array();
    int result = send_query_result(operator_fd, "alerts", "ok", NULL, alerts);
    json_decref(alerts);
    return result;
}

static int log_metrics(int operator_fd, const char *device_id,
                       json_int_t from_ts, json_int_t to_ts){
    if(!device_id)
        return send_query_result(operator_fd, "metrics_history", "error", "Missing device_id", NULL);

    json_t *metrics = db_metrics_history(device_id, (time_t)from_ts, (time_t)to_ts);
    if(!metrics) metrics = json_array();
    int result = send_query_result(operator_fd, "metrics_history", "ok", NULL, metrics);
    json_decref(metrics);
    return result;
}

static int device_details(int operator_fd, const char *device_id){
    if(!device_id) return send_query_result(operator_fd, "device_details", "error", "Missing device_id", NULL);

    int n = 0;
    conn_t *conns = server_get_conns(&n);
    conn_t *found = NULL;
    for(int i = 0; i < n; i++){
        if(conns[i].is_agent == 1 && strcmp(conns[i].device_id, device_id) == 0){
            found = &conns[i];
            break;
        }
    }

    json_t *data = NULL;
    if(found && found->online){
        data = json_object();
        json_object_set_new(data, "device_id", json_string(device_id));
        json_object_set_new(data, "type", json_string(found->type[0] ? found->type : "unknown"));
        json_object_set_new(data, "firmware", json_string(found->firmware[0] ? found->firmware : "unknown"));
        json_object_set_new(data, "cpu", json_real(found->last_cpu));
        json_object_set_new(data, "ram", json_real(found->last_ram));
        json_object_set_new(data, "uptime", json_integer(found->last_uptime));
        json_object_set_new(data, "ts", json_integer((json_int_t)found->last_seen));
        json_object_set_new(data, "online", json_true());
        if(found->last_ports)
            json_object_set(data, "ports", found->last_ports);
        else
            json_object_set_new(data, "ports", json_array());
    } else {
        data = db_device_details(device_id);
    }

    if(data){
        int result = send_query_result(operator_fd, "device_details", "ok", NULL, data);
        json_decref(data);
        return result;
    }
    return send_query_result(operator_fd, "device_details", "error", "No data", NULL);
}

static int list_devices(int operator_fd){
    json_t *data = db_list_devices();
    if(!data) data = json_array();

    int n = 0;
    conn_t *conns = server_get_conns(&n);
    for(int i = 0; i < n; i++){
        if(conns[i].is_agent != 1) continue;
        int found = 0;
        size_t idx; json_t *d;
        json_array_foreach(data, idx, d){
            json_t *id = json_object_get(d, "device_id");
            if(json_is_string(id) && strcmp(json_string_value(id), conns[i].device_id) == 0){
                json_object_set(d, "online", conns[i].online ? json_true() : json_false());
                json_object_set_new(d, "last_seen", json_integer((json_int_t)conns[i].last_seen));
                if(conns[i].type[0])
                    json_object_set_new(d, "type", json_string(conns[i].type));
                if(conns[i].firmware[0])
                    json_object_set_new(d, "firmware", json_string(conns[i].firmware));
                found = 1;
                break;
            }
        }
        if(!found){
            json_t *dev = json_object();
            json_object_set_new(dev, "device_id", json_string(conns[i].device_id));
            json_object_set_new(dev, "type", json_string(conns[i].type[0] ? conns[i].type : "unknown"));
            json_object_set_new(dev, "firmware", json_string(conns[i].firmware[0] ? conns[i].firmware : "unknown"));
            json_object_set_new(dev, "online", conns[i].online ? json_true() : json_false());
            json_object_set_new(dev, "last_seen", json_integer((json_int_t)conns[i].last_seen));
            json_array_append_new(data, dev);
        }
    }

    int result = send_query_result(operator_fd, "list_devices", "ok", NULL, data);
    json_decref(data);
    return result;
}

int Query_result(const char *buffer, MAX_EVENTS_t *ev, int operator_fd){
    (void)ev;
    json_error_t error;
    json_t *root = json_loads(buffer, 0, &error);
    if(!root){
        syslog(LOG_ERR, "Ошибка парсинга строка %d: %s\n", error.line, error.text);
        return ERR;
    }

    json_t *type = json_object_get(root, "type");
    if(!json_is_string(type) || strcmp(json_string_value(type), "Query") != 0){
        json_decref(root);
        send_query_result(operator_fd, "", "error", "Invalid type", NULL);
        return ERR;
    }

    json_t *query = json_object_get(root, "query");
    if(!json_is_string(query)){
        json_decref(root);
        send_query_result(operator_fd, "", "error", "Missing query", NULL);
        return ERR;
    }

    const char *q = json_string_value(query);
    int result = ERR;

    if(strcmp(q, "list_devices") == 0 || strcmp(q, "list_device") == 0){
        result = list_devices(operator_fd);
    } else if(strcmp(q, "device_details") == 0 || strcmp(q, "device_metrics") == 0){
        json_t *device = json_object_get(root, "device_id");
        const char *device_ID = json_is_string(device) ? json_string_value(device) : NULL;
        result = device_details(operator_fd, device_ID);
    } else if(strcmp(q, "alerts") == 0){
        result = log_alerts(operator_fd);
    } else if(strcmp(q, "metrics_history") == 0){
        json_t *j_dev  = json_object_get(root, "device_id");
        json_t *j_from = json_object_get(root, "from");
        json_t *j_to   = json_object_get(root, "to");
        const char *device_id = json_is_string(j_dev) ? json_string_value(j_dev) : NULL;
        json_int_t from_ts = json_is_integer(j_from) ? json_integer_value(j_from) : 0;
        json_int_t to_ts   = json_is_integer(j_to)   ? json_integer_value(j_to)   : (json_int_t)time(NULL);
        result = log_metrics(operator_fd, device_id, from_ts, to_ts);
    } else {
        result = send_query_result(operator_fd, q, "error", "Неизвестный тип query", NULL);
    }

    json_decref(root);
    return result;
}