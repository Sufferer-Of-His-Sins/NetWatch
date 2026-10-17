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

static void print_header(void){
    printf("\n");
    printf("╭──────────────────┬──────────┬──────────┬────────┬─────────────────────────╮\n");
    printf("│ device_id        │ type     │ firmware │ online │ last_seen               │\n");
    printf("├──────────────────┼──────────┼──────────┼────────┼─────────────────────────┤\n");
}
static void print_footer(void){
    printf("╰──────────────────┴──────────┴──────────┴────────┴─────────────────────────╯\n\n");
}
static void print_device(const char *id, const char *type, const char *firmware, int online, const char *last_seen){
    printf("│ %-16s │ %-8s │ %-8s │ %-7s │ %-23s │\n",
           id ? id : "?",
           type ? type : "?",
           firmware ? firmware : "?",
           online ? "online" : "offline",
           last_seen ? last_seen : "-");
}

/*отправка запроса на получения списка*/
int output_packets(int server_sock){
    json_t *req = json_object();
    if(!req) {return ERR;}

    json_object_set_new(req, "type", json_string("query"));
    json_object_set_new(req, "query", json_string("list_devices"));

    char *str = json_dumps(req, JSON_COMPACT);
    json_decref(req);

    if(!str) {return ERR;}
    int result = frame(server_sock, str, strlen(str));

    free(str);
    return result;
}


/*получение списка от сервера*/
int input_packets(const char *buffer){
    json_error_t error;
    json_t *root = json_loads(buffer, 0, &error);

    if(!root) {
        syslog(LOG_ERR, "Ошибка парсинга строка %d: %s\n", error.line, error.text);
        return ERR;
    }

    json_t *type = json_object_get(root, "type");
    if (!json_is_string(type) || strcmp(json_string_value(type), "Query_result") != 0) {
        syslog(LOG_ERR, "Ожидался Query_result");
        json_decref(root);
        return ERR;
    }
    
    json_t *devices = json_object_get(root, "devices");
    if (!json_is_array(devices)) {
        syslog(LOG_ERR, "Поле devices отсутствует или не массив");
        json_decref(root);
        return ERR;
    }

    print_header();
    size_t idx;
    json_t *dev;

    json_array_foreach(devices, idx, dev) {
        const char *device_id = json_string_value(json_object_get(dev, "device_id"));
        const char *dev_type  = json_string_value(json_object_get(dev, "type"));
        const char *firmware  = json_string_value(json_object_get(dev, "firmware"));
        const char *last_seen = json_string_value(json_object_get(dev, "last_seen"));

        int online = json_is_true(json_object_get(dev, "online"));

        print_device(device_id, dev_type, firmware, online ,last_seen);
    }
    print_footer();
    json_decref(root);
    return OK;
}