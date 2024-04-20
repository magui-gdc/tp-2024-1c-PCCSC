#ifndef UTILSCLIENTE_H_
#define UTILSCLIENTE_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include <string.h>
#include<readline/readline.h>
#include "enums.h"



typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;




// CONEXION CLIENTE - SERVIDOR ----- INICIACION
int crear_conexion(char*, char*);


// CONEXION CLIENTE - SERVIDOR ----- COMUNICACION
void enviar_conexion(char*, int);
void liberar_conexion(int);

///paquetes
t_paquete* crear_paquete(void);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void eliminar_paquete(t_paquete*);
///serializacion
void* serializar_paquete(t_paquete*, int);

#endif
