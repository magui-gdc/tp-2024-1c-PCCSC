#ifndef UTILSCLIENTE_H_
#define UTILSCLIENTE_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/config.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include <string.h>

typedef enum
{
	CONEXION
}op_code;

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


// CONFIGURACION
t_config* iniciar_config_cliente(char*);

// SERIALIZACION
void* serializar_paquete(t_paquete*, int);

// CONEXION CLIENTE - SERVIDOR
int crear_conexion(char*, char*);
void enviar_conexion(char*, int);
void eliminar_paquete(t_paquete*);
void liberar_conexion(int);

#endif