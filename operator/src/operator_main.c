#include "InOperator.h"

static int menu(void){
    /* для меню -> ╭, ╮, ╰, ╯, ─, │, ├, ┤*/
    printf("╭─────────────────────────────────────────────╮\n");
    printf("│                 Меню NetWatch               │\n");
    printf("╰─────────────────────────────────────────────╯\n");
    printf("╭─────────────────────────────────────────────╮\n");
    printf("│              Доступные команды              │\n");
    printf("├─────────────────────────────────────────────┤\n");
    printf("│     1.Список устройств и их статусов        │\n");
    printf("│     2.Детали устройства (последние метрики) │\n");
    printf("│     3.История метрик за период              │\n");
    printf("│     4.Журнал тревог                         │\n");
    printf("│     5.Отправка команды устройству           │\n");
    printf("│     6.Live-режим                            │\n");
    printf("│     0.Выход                                 │\n");
    printf("╰─────────────────────────────────────────────╯");
    printf("\n\nВыберите пункт: ");
}

static json_t *wait_type(int fd, const char *want) {
    while (1) {
        char *body = recv_frame(fd);
        if (!body) return NULL;
        json_error_t err;
        json_t *root = json_loads(body, 0, &err);
        free(body);
        if (!root) continue;
        json_t *t = json_object_get(root, "type");
        if (json_is_string(t) && strcmp(json_string_value(t), want) == 0)
            return root;
        if (json_is_string(t) && strcmp(json_string_value(t), "Alert") == 0)
            Alert(json_dumps(root, 0)); /* best-effort print */
        json_decref(root);
    }
}

static int do_list(int sock) {
    if (output_packets(sock) != OK) return ERR;
    json_t *root = wait_type(sock, "Query_result");
    if (!root) return ERR;
    char *s = json_dumps(root, JSON_COMPACT);
    if (s) {
        input_packets(s);
        free(s);
    }
    json_decref(root);
    return OK;
}

static int do_details(int sock) {
    char id[64];
    printf("device_id: ");
    if (!fgets(id, sizeof(id), stdin)) return ERR;
    id[strcspn(id, "\n")] = 0;
    if (send_device_metrics(sock, id) != OK) return ERR;
    return Get_device_metrics(sock, id);
}

static int do_history(int sock) {
    char id[64];
    printf("device_id: ");
    if (!fgets(id, sizeof(id), stdin)) return ERR;
    id[strcspn(id, "\n")] = 0;
    time_t from, to;
    if (read_period(&from, &to) != OK) return ERR;
    if (send_metrics_history(sock, id, from, to) != OK) return ERR;
    return Get_metrics_history(sock, id);
}

static int do_alerts(int sock) {
    json_t *req = json_object();
    json_object_set_new(req, "type", json_string("Query"));
    json_object_set_new(req, "query", json_string("alerts"));
    char *s = json_dumps(req, JSON_COMPACT);
    json_decref(req);
    if (!s) return ERR;
    send_frame(sock, s, strlen(s));
    free(s);

    json_t *root = wait_type(sock, "Query_result");
    if (!root) return ERR;
    json_t *data = json_object_get(root, "data");
    if (!json_is_array(data) || json_array_size(data) == 0)
        printf("Тревог нет\n");
    else {
        size_t i; json_t *a;
        json_array_foreach(data, i, a) {
            char *tmp = json_dumps(a, JSON_COMPACT);
            if (tmp) { Alert(tmp); free(tmp); }
        }
    }
    json_decref(root);
    return OK;
}

static int do_cmd(int sock) {
    char id[64], cmd[32];
    printf("device_id: ");
    if (!fgets(id, sizeof(id), stdin)) return ERR;
    id[strcspn(id, "\n")] = 0;
    printf("команда (reboot/get_status/set_interval): ");
    if (!fgets(cmd, sizeof(cmd), stdin)) return ERR;
    cmd[strcspn(cmd, "\n")] = 0;

    if (strcmp(cmd, "reboot") == 0) {
        send_reboot(sock, id);
        return Get_reboot(sock);
    } else if (strcmp(cmd, "get_status") == 0) {
        send_get_status(sock, id);
        return Get_get_status(sock);
    } else if (strcmp(cmd, "set_interval") == 0) {
        int iv;
        printf("interval: ");
        if (scanf("%d", &iv) != 1) return ERR;
        getchar();
        send_set_interval(sock, id, iv);
        return Get_interval(sock);
    }
    printf("Неизвестная команда\n");
    return ERR;
}

static int do_live(int sock) {
    printf("Live-режим\n");
    while (1) {
        char *body = recv_frame(sock);
        if (!body) { printf("Соединение потеряно\n"); return ERR; }
        Alert(body);
        free(body);
    }
    return OK;
}

int Operator_m(void) {
    const char *host = getenv("NETWATCH_HOST");
    if (!host) host = DEFAULT_SERVER_HOST;
    int port = DEFAULT_SERVER_PORT;
    const char *ep = getenv("NETWATCH_PORT");
    if (ep) port = atoi(ep);

    printf("Подключение к %s:%d ...\n", host, port);
    int sock = operator_connect(host, port);
    if (sock < 0) {
        fprintf(stderr, "Не удалось подключиться\n");
        return ERR;
    }
    printf("Подключено.\n\n");

    while (1) {
        menu();
        int input;
        if (scanf("%d", &input) != 1) {
            while (getchar() != '\n');
            continue;
        }
        getchar();
        switch (input) {
            case 1: do_list(sock); break;
            case 2: do_details(sock); break;
            case 3: do_history(sock); break;
            case 4: do_alerts(sock); break;
            case 5: do_cmd(sock); break;
            case 6: do_live(sock); break;
            case 0: close(sock); return OK;
            default: printf("Неверный выбор\n"); break;
        }
    }
}