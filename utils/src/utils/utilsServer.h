#ifndef UTILSSERVER_H_
#define UTILSSERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <string.h>
#include <pthread.h>
#include "enums.h"

extern t_log* logger;

// CONEXION CLIENTE - SERVIDOR
void* recibir_buffer(int*, int);

int iniciar_servidor(char*);
int esperar_cliente(int);
void recibir_conexion(int);
int recibir_operacion(int);
void* servidor_escucha(void*);
void* atender_cliente(void*);

#endif