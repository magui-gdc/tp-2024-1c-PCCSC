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

typedef struct{ // SIEMPRE CARGADO EN MEMORIA
    t_tabla_paginas** tabla_paginas;
    int cant_tablas;
    bool presence;
} t_lista_tablas;

typedef struct{
    u_int32_t pid;  //PID DEL PROCESO ASOCIADO A LA TABLA
    t_pagina** paginas;
} t_tabla_paginas;

typedef struct{
    u_int32_t frame;   //checkear tipo de dato
    u_int32_t offset;  //checkear tipo de dato
    bool presence;
} t_pagina;

typedef struct{
    u_int32_t pid;
    int pagina;
} t_frame;

extern config_struct config;
extern void* memoria;   //el "espacio de usario", la "memoria real"
extern t_lista_tablas lista_tablas;
extern t_frame** frames_libres; //"bitmap" de frames libres 
extern int cant_frames = 0;

void cargar_config_struct_MEMORIA(t_config*);

// inicializacion de las estructuras necesarias para la paginacion
void init_memoria();
void init_paginacion();
void init_lista_tablas();
void free_lista_tablas();
void free_tabla_paginas(t_tabla_paginas);
void free_pagina(t_pagina);
void init_bitmap_frames();

void crear_proceso(int,char*);
void eliminar_proceso(int);

t_pagina new_pagina();
t_tabla_paginas new_tabla_paginas();
bool pagina_valida(t_pagina);
bool pagina_presente(t_pagina); //consulta pagina cargada en memoria

int consulta_marco(t_pagina);
int buscar_marco_libre();

void aplicar_retardo(); 
