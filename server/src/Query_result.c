#include "InServer.h"

int Query_result(const char *buffer, MAX_EVENTS_t *ev, int fd){
    json_error_t error;
    json_t *root = json_loads(buffer, 0, &error);
    
    if(!root){
        syslog(LOG_ERR, "Ошибка парсинга строка %d: %s\n", error.line, error.text);
        return ERR;
    }
    json_t *type = json_object_get(root, "type");

    if(json_is_string(type) && strcmp(json_string_value(type), "Query_result") == 0){
        // Обработка запроса
    }

    json_decref(root);
    return OK;
}