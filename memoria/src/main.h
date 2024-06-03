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

typedef struct {
    void* espacio_usuario;
    int num_frames;
    int* tabla_paginas;
} t_memoria;

extern t_memoria memoria;
extern config_struct config;

void cargar_config_struct_MEMORIA(t_config*);

/////// funciones memoria
void inicializar_memoria();
void crear_proceso(int,char*);
void encontrar_hueco(); //funcion auxiliar para encontrar un espacio libre en la tabla de paginas uwu
void eliminar_proceso(int);