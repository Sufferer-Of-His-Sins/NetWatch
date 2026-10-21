#include "InOperator.h"
/*команды которые нужно отправлять get_status, reboot, set_interval*/


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
static int check_result(json_t *root, const char *command){
    json_t *status = json_object_get(root, "status");
    json_t *reason = json_object_get(root, "reason");
    if(!json_is_string(status)) {return ERR;}

    const char *text = json_is_string(reason) ? json_string_value(reason) : "";

    if(strcmp(json_string_value(status), "SUCCESS") != 0){
        printf("%s: ошибка: %s\n", command, text);
        return ERR;
    }
    printf("%s: %s\n", command, text);
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
static json_t *wait_command_result(int fd){
    while(1){
        char *body = read_frame(fd);
        if(!body) {return NULL;}

        json_error_t error;
        json_t *root = json_loads(body, 0, &error);
        free(body);
        if(!root) {continue;}

        json_t *type = json_object_get(root, "type");
        if(json_is_string(type) && strcmp(json_string_value(type), "Command_result") == 0){
            return root;
        }
        json_decref(root);
    }
}

/*Отправка команды*/
int send_reboot(int server_socket, const char *device_id){
    json_t *req = json_object();
    if (!req) {return ERR;}

    json_object_set_new(req, "type", json_string("Command_request"));
    json_object_set_new(req, "device_id", json_string(device_id));
    json_object_set_new(req, "command", json_string("reboot"));

    char *str = json_dumps(req, JSON_COMPACT);
    json_decref(req);
    if (!str) {return ERR;}

    int result = frame(server_socket, str, strlen(str));
    free(str);
    return result;
}
int send_set_interval(int server_socket, const char *device_id,int interval){
    if(interval < 1 || interval > 3600) {return ERR;}

    json_t *req = json_object();
    if (!req) {return ERR;}

    json_object_set_new(req, "type", json_string("Command_request"));
    json_object_set_new(req, "device_id", json_string(device_id));
    json_object_set_new(req, "command", json_string("set_interval"));
    json_object_set_new(req, "interval", json_integer(interval));

    char *str = json_dumps(req, JSON_COMPACT);
    json_decref(req);
    if (!str) {return ERR;}
    int result = frame(server_socket, str, strlen(str));

    free(str);
    return result;
}
int send_get_status(int server_socket, const char *device_id){
    json_t *req = json_object();
    if (!req) {return ERR;}

    json_object_set_new(req, "type", json_string("Command_request"));
    json_object_set_new(req, "device_id", json_string(device_id));
    json_object_set_new(req, "command", json_string("get_status"));

    char *str = json_dumps(req, JSON_COMPACT);
    json_decref(req);
    if (!str) {return ERR;}

    int result = frame(server_socket, str, strlen(str));
    free(str);
    return result;
}

/*Принятие команд*/
int Get_get_status(int server_socket){
    json_t *root = wait_command_result(server_socket);
    if(!root) {return ERR;}

    int rc = check_result(root, "get_status");

    json_t *data = json_object_get(root, "data");
    if(rc == OK && data){
        char *str = json_dumps(data, JSON_INDENT(2));
        if(str){
            printf("%s\n", str);
            free(str);
        }
    }
    json_decref(root);
    return rc;
}

int Get_reboot(int server_socket){
    json_t *root = wait_command_result(server_socket);
    if(!root) {return ERR;}

    int rc = check_result(root, "reboot");
    json_decref(root);
    return rc;
}

int Get_interval(int server_socket){
    json_t *root = wait_command_result(server_socket);
    if(!root) {return ERR;}

    int rc = check_result(root, "set_interval");
    json_decref(root);
    return rc;
}