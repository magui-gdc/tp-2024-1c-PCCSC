#ifndef UTILSCLIENTE_H_
#define UTILSCLIENTE_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<sys/socket.h>
#include<netdb.h>
#include <string.h>

int crear_conexion(char* ip, char* puerto);

#endif