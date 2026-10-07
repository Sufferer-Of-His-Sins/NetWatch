#include "InServer.h"

int Heartbeat_ack(int socket_fd){
    const char *ping_message = "PING";
    char buffer[sizeof(ping_message)];

    if(send(socket_fd, ping_message, strlen(ping_message), 0) == ERR){
        syslog(LOG_ERR, "Ошибка отправки PING: %s\n", strerror(errno));
        return false;
    }

    ssize_t bytes_received = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
    if(bytes_received == ERR){
        return true;
    }
    return false;
}