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

// CONEXION CLIENTE - SERVIDOR ---- INICIACION
int iniciar_servidor(char*, t_log*);
int esperar_cliente(int);

// CONEXION CLIENTE - SERVIDOR ---- COMUNICACION
//operacion
int recibir_operacion(int);
void* recibir_buffer(int*, int);

///conexion (mensajes)
void recibir_conexion(int);

///paquetes
t_list* recibir_paquete(int);
void iterator(char*value);


//threads
void* servidor_escucha(void*);
void* atender_cliente(void*);


#endif
