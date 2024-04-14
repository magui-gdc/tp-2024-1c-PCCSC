#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/config.h>

/*
PUERTO_ESCUCHA=8002
TAM_MEMORIA=4096
TAM_PAGINA=32
PATH_INSTRUCCIONES=/home/utnso/scripts-pruebas
RETARDO_RESPUESTA=1000
*/

typedef struct {
    char* puerto_escucha;
    char* tam_memoria;
    char* tam_pagina;
    char* path_instrucciones;
    char* retardo_respuesta
} config_struct;

extern config_struct config;

void cargar_config_struct_MEMORIA(t_config*);
