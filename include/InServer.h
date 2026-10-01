#ifndef SERVER_H
#define SERVER_H

#include "library.h"


int Server(void);
int Register_ack(void);
int Heartbeat_ack(void);
int Command(void);
int Alert(void);
int Query_result(void);

#endif