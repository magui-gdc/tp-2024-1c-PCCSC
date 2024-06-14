#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/buffer.h>

typedef struct {
    char* puerto_escucha;
    int tam_memoria;
    int tam_pagina;
    char* path_instrucciones;
    int retardo_respuesta;
} config_struct;

extern config_struct config;

void cargar_config_struct_MEMORIA(t_config*);

typedef struct{ // para el "bitmap" de frames libres 
    u_int32_t pid;
    int pagina;
    bool presence;
} t_frame;

typedef struct{         // y donde van los datos ahre??????
    t_frame* frame;   //checkear tipo de dato
    u_int32_t offset;  //checkear tipo de dato
    //agregar un PAGE ID???
    bool presence;
} t_pagina;

typedef struct{
    u_int32_t pid;  //PID DEL PROCESO ASOCIADO A LA TABLA
    u_int32_t tid;  // el TABLE ID jajaja
    t_list* paginas;
    int cant_paginas;
} t_tabla_paginas;

typedef struct{ // SIEMPRE CARGADO EN MEMORIA
    t_list* tabla_paginas;
    int cant_tablas;
} t_lista_tablas;


typedef struct{
    u_int32_t pid;
    u_int32_t tid;
    //e_estado_proceso estado; //mm maybe?? si se usa poner el include necesario!!!
} t_pseudo_pcb;


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

extern void* memoria;   // el "espacio de usario", la "memoria real"
extern t_lista_tablas *lista_tablas;
extern u_int32_t tid; //contador para el TABLE IDENTIFICATOR jaja

extern t_list* bitmap_frames_libres; //"bitmap" de paginas cargadas
extern t_list* lista_procesos; //lista de procesos con sus respectivas listas de paginas

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


/*          NACIMIENTO            */
void init_memoria();
void init_paginacion();

void init_lista_tablas(); //inicializa la lista de tablas y devuelve el puntero
void init_bitmap_frames();
void init_lista_procesos();

int create_tabla_paginas(u_int32_t pid); //inserta nueva tabla en lista de paginas
int create_pagina(t_list *tabla_paginas); //le mandas la lista de la pagina e inserta la nueva pagina
int add_psuedo_pcb(u_int32_t pid, u_int32_t tid); 


/*          MUERTE            */
void free_lista_tablas();
void free_tabla_paginas(t_tabla_paginas);
void free_pagina(t_pagina);

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

bool pagina_valida(t_pagina); 
bool pagina_presente(t_pagina); //consulta pagina cargada en memoria
t_frame* consulta_marco(t_pagina); //entra pagina sale puntero al marco??
int buscar_marco_libre();
