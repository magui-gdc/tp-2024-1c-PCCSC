#ifndef UTILSSERVER_H_
#define UTILSSERVER_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include <string.h>

extern t_log* logger;

int iniciar_servidor(char*);
int esperar_cliente(int);

#endif