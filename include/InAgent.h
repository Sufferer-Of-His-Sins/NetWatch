#ifndef INAGENT_H
#define INAGENT_H

#include "library.h"


int Agent_Register(void);
int Metrics(void);
int Heartbeat(void);
int Command_result(void);
int Query(void);

#endif