#include "InOperator.h"

static int form(json_t *root, json_t *type, json_t *dev_id, json_t *kind, json_t *ts, json_t *details){
    if (!json_is_string(type) || strcmp(json_string_value(type), "alert") != 0) {
        syslog(LOG_ERR, "Alert: неожиданный type");
        return ERR;
    }
    
    /* для тревог -> ╭, ╮, ╰, ╯, ─, │, ├, ┤*/
    printf("\n");
    printf("╭──────────────────────────────────────╮\n");
    printf("│              Тревога                 │\n");
    printf("├──────────────────────────────────────┤\n");
    
    if(json_is_string(dev_id)){
        printf("│ Устройство : %-23s │\n", json_string_value(dev_id));
    }
    if(json_is_string(kind)){
        if(strcmp(json_string_value(kind), "device_down") == 0)      {printf("│ Тип        : %-23s │\n", json_string_value(kind));}
        else if(strcmp(json_string_value(kind), "high_cpu") == 0)    {printf("│ Тип        : %-23s │\n", json_string_value(kind));}
        else if(strcmp(json_string_value(kind), "high_ram") == 0)    {printf("│ Тип        : %-23s │\n", json_string_value(kind));}
        else                                                         {printf("│ Тип        : %-23s │\n", json_string_value(kind));}
    }
    if(json_is_integer(ts)){
        printf("│ Время      : %-23lld │\n", (long long)json_integer_value(ts));
    }
    if(json_is_object(details)){
        const char *key;
        json_t *value;

        printf("├──────────────────────────────────────┤\n");
        printf("│ Доп. информация                      │\n");
        json_object_foreach(details, key, value){
            if(json_is_string(value)){
                printf("│   %-12s : %-16s │\n", key, json_string_value(value));
            }else if (json_is_integer(value)) {
                printf("│   %-12s : %-16lld │\n", key, (long long)json_integer_value(value));
            }else if (json_is_real(value)) {
                printf("│   %-12s : %-16.2f │\n", key, json_real_value(value));
            }else if (json_is_boolean(value)) {
                printf("│   %-12s : %-16s │\n", key, json_is_true(value) ? "true" : "false");
            }
        }
        
    }
    return OK;
}

int Alert(const char *buffer){
    json_error_t error;
    json_t *root = json_loads(buffer, 0, &error);

    if(!root){
        syslog(LOG_ERR, "Command: ошибка парсинга строка %d: %s\n", error.line, error.text);
        return ERR;
    }
    
    json_t *type = json_object_get(root, "type");
    json_t *dev_id = json_object_get(root, "device_id");
    json_t *kind = json_object_get(root, "kind");
    json_t *ts = json_object_get(root, "ts");
    json_t *details = json_object_get(root, "details");

    int result = form(root, type, dev_id, kind, ts, details);

    json_decref(root);
    return result;
}