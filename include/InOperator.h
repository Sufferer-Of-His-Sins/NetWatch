#ifndef INOPERATOR_H
#define INOPERATOR_H

#include "library.h"

int output_packets(int server_sock);
int input_packets(const char *buffer);
int send_device_metrics(int server_socket, const char *device_id);
int Get_device_metrics(int server_socket, const char *device_id);
int read_period(time_t *from, time_t *to);
int send_metrics_history(int server_socket, const char *device_id, time_t from, time_t to);
int Get_metrics_history(int server_socket, const char *device_id);
int Alert(const char *buffer);
int Operator_m(void);

int send_frame(int fd, const char *body, size_t body_len);
char *recv_frame(int fd);
int operator_connect(const char *host, int port);

int send_reboot(int server_socket, const char *device_id);
int send_set_interval(int server_socket, const char *device_id, int interval);
int send_get_status(int server_socket, const char *device_id);
int Get_get_status(int server_socket);
int Get_reboot(int server_socket);
int Get_interval(int server_socket);

#endif