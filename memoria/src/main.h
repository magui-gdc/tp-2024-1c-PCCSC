#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/buffer.h>

/*
PUERTO_ESCUCHA=8002
TAM_MEMORIA=4096                    este es el tamaño de la memoria fisica
TAM_PAGINA=32                       tamaño de cada pagina, 
PATH_INSTRUCCIONES=/home/utnso/scripts-pruebas
RETARDO_RESPUESTA=1000
*/

typedef struct {
    char* puerto_escucha;
    char* tam_memoria;
    char* tam_pagina;
    char* path_instrucciones;
    char* retardo_respuesta;
} config_struct;

extern config_struct config;

void cargar_config_struct_MEMORIA(t_config*);

void crear_proceso(int,char*);
void eliminar_proceso(int);
void aplicar_retardo();

//  FUNCIONES ACCESO A ESPACIO USUARIO
/*  
    LEER MEMORIA
    devolver el valor que se encuentra a partir de la dirección física pedida.
*/

char* leer_memoria(int, int, uint32_t);