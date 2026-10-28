#include "InOperator.h"


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
static int read_all(int fd, void *buf, size_t len){
    size_t got = 0;
    while(got < len){
        ssize_t n = read(fd, (char *)buf + got, len - got);
        if(n == 0){
            syslog(LOG_ERR, "Сервер закрыл соединение\n");
            return ERR;
        }
        if(n < 0){
            if(errno == EINTR) {continue;}
            syslog(LOG_ERR, "Ошибка чтения: %s\n", strerror(errno));
            return ERR;
        }
        got += n;
    }
    return OK;
}
static char *read_frame(int fd){
    uint32_t len;
    if(read_all(fd, &len, sizeof(len)) != OK) {return NULL;}

    len = ntohl(len);
    if(len == 0 || len > MAX_FRAME_HISTORY){
        syslog(LOG_ERR, "Неверная длина кадра: %u\n", len);
        return NULL;
    }

    char *body = malloc(len + 1);
    if(!body) {return NULL;}

    if(read_all(fd, body, len) != OK){
        free(body);
        return NULL;
    }
    body[len] = '\0';
    return body;
}
static json_t *wait_query_result(int fd){
    while(1){
        char *body = read_frame(fd);
        if(!body) {return NULL;}

        json_error_t error;
        json_t *root = json_loads(body, 0, &error);
        free(body);
        if(!root) {continue;}

        json_t *type = json_object_get(root, "type");
        if(json_is_string(type) && strcmp(json_string_value(type), "Query_result") == 0){
            return root;
        }
        json_decref(root);
    }
}
static int parse_datetime(const char *str, time_t *out){
    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    char *end = strptime(str, "%Y-%m-%d %H:%M", &tm);
    if(!end || *end != '\0') {return ERR;}

    tm.tm_isdst = -1;
    time_t t = mktime(&tm);
    if(t == (time_t)-1) {return ERR;}

    *out = t;
    return OK;
}

/*Спрашивает у пользователя период, возвращает OK если ввод корректный*/
int read_period(time_t *from, time_t *to){
    char line[64];

    printf("С (ГГГГ-ММ-ДД ЧЧ:ММ): ");
    if(!fgets(line, sizeof(line), stdin)) {return ERR;}
    line[strcspn(line, "\n")] = '\0';
    if(parse_datetime(line, from) != OK){
        printf("Неверный формат даты\n");
        return ERR;
    }

    printf("По (ГГГГ-ММ-ДД ЧЧ:ММ): ");
    if(!fgets(line, sizeof(line), stdin)) {return ERR;}
    line[strcspn(line, "\n")] = '\0';
    if(parse_datetime(line, to) != OK){
        printf("Неверный формат даты\n");
        return ERR;
    }

    if(*from >= *to){
        printf("Начало периода должно быть раньше конца\n");
        return ERR;
    }
    return OK;
}



/*Отправка запроса*/
int send_metrics_history(int server_socket, const char *device_id, time_t from, time_t to){
    if(from >= to) {return ERR;}

    json_t *req = json_object();
    if (!req) {return ERR;}

    json_object_set_new(req, "type", json_string("Query"));
    json_object_set_new(req, "query", json_string("metrics_history"));
    json_object_set_new(req, "device_id", json_string(device_id));
    json_object_set_new(req, "from", json_integer((json_int_t)from));
    json_object_set_new(req, "to", json_integer((json_int_t)to));
    json_object_set_new(req, "limit", json_integer(HISTORY_LIMIT));

    char *str = json_dumps(req, JSON_COMPACT);
    json_decref(req);
    if (!str) {return ERR;}

    int result = frame(server_socket, str, strlen(str));
    free(str);
    return result;
}



/*Вывод истории*/
static void print_row(json_t *row){
    char timebuf[32] = "?";

    json_t *ts = json_object_get(row, "ts");
    if(json_is_integer(ts)){
        time_t t = (time_t)json_integer_value(ts);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    } else if(json_is_string(ts)){
        snprintf(timebuf, sizeof(timebuf), "%s", json_string_value(ts));
    }

    json_t *cpu = json_object_get(row, "cpu");
    json_t *ram = json_object_get(row, "ram");
    json_t *uptime = json_object_get(row, "uptime");

    int up = 0, total = 0;
    json_t *ports = json_object_get(row, "ports");
    if(json_is_array(ports)){
        size_t i;
        json_t *port;
        json_array_foreach(ports, i, port){
            json_t *state = json_object_get(port, "state");
            total++;
            if(json_is_string(state) && strcmp(json_string_value(state), "up") == 0) {up++;}
        }
    }

    printf("%-20s %6.1f %6.1f %10lld   %d/%d\n",timebuf,
           json_is_number(cpu) ? json_number_value(cpu) : 0.0,
           json_is_number(ram) ? json_number_value(ram) : 0.0,
           json_is_integer(uptime) ? (long long)json_integer_value(uptime) : 0LL,up, total);
}



/*Принятие ответа*/
int Get_metrics_history(int server_socket, const char *device_id){
    json_t *root = wait_query_result(server_socket);
    if(!root) {return ERR;}

    json_t *status = json_object_get(root, "status");
    json_t *reason = json_object_get(root, "reason");
    json_t *data = json_object_get(root, "data");

    if(!json_is_string(status)){
        json_decref(root);
        return ERR;
    }

    const char *st = json_string_value(status);
    if (strcmp(st, "SUCCESS") != 0 && strcmp(st, "ok") != 0) {
        printf("Не удалось получить историю: %s\n",json_is_string(reason) ? json_string_value(reason) : "неизвестная ошибка");
        json_decref(root);
        return ERR;
    }

    if(!json_is_array(data) || json_array_size(data) == 0){
        printf("За указанный период для %s метрик нет\n", device_id);
        json_decref(root);
        return OK;
    }

    printf("\n=== История метрик %s ===\n", device_id);
    printf("%-20s %6s %6s %10s   %s\n", "Время", "CPU%", "RAM%", "Uptime,с", "Порты");

    size_t i;
    json_t *row;
    json_array_foreach(data, i, row){print_row(row);}

    printf("Записей: %zu", json_array_size(data));
    if(json_array_size(data) >= HISTORY_LIMIT){printf(" (достигнут лимит, сузьте период)");}
    printf("\n\n");

    json_decref(root);
    return OK;
}