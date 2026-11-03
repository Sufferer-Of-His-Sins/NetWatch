#include "InServer.h"

static int frame(int fd, const char *body, size_t body_len){
    uint32_t net_len = htonl((uint32_t)body_len);
    if (send(fd, &net_len, 4, 0) != 4) {
        syslog(LOG_ERR, "Register: ошибка отправки длины: %s\n", strerror(errno));
        return ERR;
    }
    if (send(fd, body, body_len, 0) != (ssize_t)body_len) {
        syslog(LOG_ERR, "Register: ошибка отправки тела: %s\n", strerror(errno));
        return ERR;
    }
    return OK;
}

static int response_err(int agent_fd, const char *reason){
    json_t *obj = json_object();
    if(!obj) {return ERR;}

    json_object_set_new(obj,"type", json_string("Register_ask"));
    json_object_set_new(obj, "status", json_string("ERROR"));
    json_object_set_new(obj, "reason", json_string("Server is full"));

    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    
    if(str == NULL){return ERR;}
    int rc = frame(agent_fd, str, strlen(str));
    
    free(str);
    return rc;
}
static int response_ok(int agent_fd){
    json_t *obj = json_object();
    if(!obj) {return ERR;}

    json_object_set_new(obj, "type", json_string("Register_ack"));
    json_object_set_new(obj, "status", json_string("SUCCESS"));
    json_object_set_new(obj, "reason", json_string("Registration successful"));
    
    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    
    if(str == NULL){return ERR;}
    int rc = frame(agent_fd, str, strlen(str));
    
    free(str);
    return rc;
}


int Register_ack(const char *buffer, MAX_EVENTS_t *ev, int agent_fd){
    json_error_t error;
    json_t *root = json_loads(buffer, 0, &error);
    
    if(!root){
        syslog(LOG_ERR, "Ошибка парсинга строка %d: %s\n", error.line, error.text);
        return ERR;
    }

    int result = OK;
    json_t *type = json_object_get(root, "type");

    if(json_is_string(type) && strcmp(json_string_value(type), "Agent_Register") == 0){
        if(UP_amount_size(ev) == ERR){
            result = response_err(agent_fd, "Server is full");
        } else {
            result = response_ok(agent_fd);
            syslog(LOG_INFO, "Agent registered on fd=%d\n", agent_fd);
        }
    } else {
        result = response_err(agent_fd, "Invalid register message");
    }

    json_decref(root);
    return result;
}