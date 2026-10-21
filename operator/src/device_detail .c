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
/*Читает один кадр, результат нужно освободить через free*/
static char *read_frame(int fd){
    uint32_t len;
    if(read_all(fd, &len, sizeof(len)) != OK) {return NULL;}

    len = ntohl(len);
    if(len == 0 || len > 65536){
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
/*Ждёт Query_result, другие пакеты (например Alert) пропускает*/
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



/*Отправка запроса*/
int send_device_metrics(int server_socket, const char *device_id){
    json_t *req = json_object();
    if (!req) {return ERR;}

    json_object_set_new(req, "type", json_string("Query"));
    json_object_set_new(req, "query", json_string("device_metrics"));
    json_object_set_new(req, "device_id", json_string(device_id));

    char *str = json_dumps(req, JSON_COMPACT);
    json_decref(req);
    if (!str) {return ERR;}

    int result = frame(server_socket, str, strlen(str));
    free(str);
    return result;
}



/*Вывод метрик*/
static void print_uptime(long long sec){
    long long d = sec / 86400;
    long long h = (sec % 86400) / 3600;
    long long m = (sec % 3600) / 60;
    printf("Uptime: %lldд %lldч %lldм\n", d, h, m);
}

static void print_metrics(const char *device_id, json_t *data){
    printf("\n--- Устройство %s ---\n", device_id);

    json_t *type = json_object_get(data, "type");
    json_t *firmware = json_object_get(data, "firmware");
    if(json_is_string(type)) {printf("Тип: %s\n", json_string_value(type));}
    if(json_is_string(firmware)) {printf("Прошивка: %s\n", json_string_value(firmware));}

    json_t *ts = json_object_get(data, "ts");
    if(json_is_integer(ts)){
        time_t t = (time_t)json_integer_value(ts);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
        printf("Время замера: %s\n", buf);
    } else if(json_is_string(ts)){
        printf("Время замера: %s\n", json_string_value(ts));
    }

    json_t *cpu = json_object_get(data, "cpu");
    json_t *ram = json_object_get(data, "ram");
    json_t *uptime = json_object_get(data, "uptime");

    if(json_is_number(cpu)) {printf("CPU: %.1f %%\n", json_number_value(cpu));}
    if(json_is_number(ram)) {printf("RAM: %.1f %%\n", json_number_value(ram));}
    if(json_is_integer(uptime)) {print_uptime(json_integer_value(uptime));}

    json_t *ports = json_object_get(data, "ports");
    if(json_is_array(ports)){
        printf("Порты:\n");
        size_t i;
        json_t *port;
        json_array_foreach(ports, i, port){
            json_t *id = json_object_get(port, "id");
            json_t *state = json_object_get(port, "state");
            json_t *rx = json_object_get(port, "rx");
            json_t *tx = json_object_get(port, "tx");

            printf("  порт %lld: %-4s rx=%lld tx=%lld\n",
                   json_is_integer(id) ? json_integer_value(id) : 0,
                   json_is_string(state) ? json_string_value(state) : "?",
                   json_is_integer(rx) ? json_integer_value(rx) : 0,
                   json_is_integer(tx) ? json_integer_value(tx) : 0);
        }
    }
    printf("\n");
}



/*Принятие ответа*/
int Get_device_metrics(int server_socket, const char *device_id){
    json_t *root = wait_query_result(server_socket);
    if(!root) {return ERR;}

    json_t *status = json_object_get(root, "status");
    json_t *reason = json_object_get(root, "reason");
    json_t *data = json_object_get(root, "data");

    if(!json_is_string(status)){
        json_decref(root);
        return ERR;
    }

    if(strcmp(json_string_value(status), "SUCCESS") != 0){
        printf("Не удалось получить метрики: %s\n",
               json_is_string(reason) ? json_string_value(reason) : "неизвестная ошибка");
        json_decref(root);
        return ERR;
    }

    if(!json_is_object(data)){
        printf("Для устройства %s нет метрик\n", device_id);
        json_decref(root);
        return ERR;
    }

    print_metrics(device_id, data);
    json_decref(root);
    return OK;
}