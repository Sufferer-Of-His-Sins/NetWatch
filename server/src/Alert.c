#include "InServer.h"

int Alert(int socket_fd){
    const char *alert_message[] = "ERROR: ALERT... ALERT";
    char buffer[sizeof(alert_message)];

    if(send(socket_fd, alert_message, strlen(alert_message), 0) == ERR){
        syslog(LOG_ERR, "Ошибка отправки ALERT: %s\n", strerror(errno));
        return ERR;
    }
    return 0;
}