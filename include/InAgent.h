#ifndef INAGENT_H
#define INAGENT_H

#include "library.h"

int send_frame(int fd, const char *body, size_t body_len);
char *recv_frame(int fd);
void agent_init(agent_t *a);
void agent_cleanup(agent_t *a);
int agent_connect(agent_t *a);
int agent_register(agent_t *a);
void agent_update_metrics(agent_t *a);
int agent_send_metrics(agent_t *a);
int agent_send_heartbeat(agent_t *a);
int agent_handle_command(agent_t *a, json_t *root);
int agent_process_incoming(agent_t *a);

#endif