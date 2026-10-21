#ifndef INOPERATOR_H
#define INOPERATOR_H

#include "library.h"


int output_packets(int server_sock);
int input_packets(const char *buffer);
int Alert(const char *buffer);
int Operator_m(void);
#endif