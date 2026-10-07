#include "InServer.h"

static int response_err(int fd){
    json_t *obj = json_object();
    
    json_object_set_new(obj,"type", json_string("Register_ask"));
    json_object_set_new(obj, "status", json_string("ERROR"));
    json_object_set_new(obj, "reason", json_string("Server is full"));

    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    
    if(str == NULL){return ERR;}
    size_t len = strlen(str);
    ssize_t sent = send(fd, str, len, 0);
    free(str);

    return (sent == (ssize_t)len) ? OK : ERR;
}
static int response_ok(int fd){
    json_t *obj = json_object();

    json_object_set_new(obj, "type", json_string("Register_ack"));
    json_object_set_new(obj, "status", json_string("SUCCESS"));
    json_object_set_new(obj, "reason", json_string("Registration successful"));
    
    char *str = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    
    if(str == NULL){return ERR;}
    size_t len = strlen(str);
    ssize_t sent = send(fd, str, len, 0);
    free(str);

    return (sent == (ssize_t)len) ? OK : ERR;
}


int Register_ack(const char *buffer, MAX_EVENTS_t *ev, int fd){
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
            result = ERR;
        } else {
            result = response_ok(fd);
        }
    }

    json_decref(root);
    return result;
}