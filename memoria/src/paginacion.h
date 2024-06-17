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
    uint32_t pid;
    int pagina;
    bool presence;
} t_frame;

typedef struct{         // y donde van los datos ahre??????
    t_frame* frame;   //checkear tipo de dato
    uint32_t offset;  //checkear tipo de dato
    //agregar un PAGE ID???
    bool presence;
} t_pagina;

typedef struct{
    uint32_t pid;  //PID DEL PROCESO ASOCIADO A LA TABLA
    uint32_t tid;  // el TABLE ID jajaja
    char* path_proceso; // path de las instrucciones
    t_list* paginas; // t_paginas*
    int cant_paginas;
} t_tabla_paginas; // POR PROCESO: todo: ya con esto podrías asociar proceso con tabla de páginas sin t_pseudo_pcb

typedef struct{ // SIEMPRE CARGADO EN MEMORIA
    t_list* tabla_paginas;
    int cant_tablas;
} t_lista_tablas;


typedef struct{
    uint32_t pid;
    uint32_t tid;
    //e_estado_proceso estado; //mm maybe?? si se usa poner el include necesario!!!
} t_pseudo_pcb;


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

extern void* memoria;   // el "espacio de usario", la "memoria real"
extern t_lista_tablas *lista_tablas;
extern uint32_t tid; //contador para el TABLE IDENTIFICATOR jaja

extern t_list* bitmap_frames_libres; //"bitmap" de paginas cargadas
extern t_list* lista_procesos; //lista de procesos con sus respectivas listas de paginas

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


/*          NACIMIENTO            */
void init_memoria();
void init_paginacion();

void init_lista_tablas(); //inicializa la lista de tablas y devuelve el puntero
void init_bitmap_frames();
void init_lista_procesos();
uint32_t create_tabla_paginas(uint32_t pid); //inserta nueva tabla en lista de paginas, devuelve el TID
void create_pagina(t_list*); //le mandas la lista de la pagina e inserta la nueva pagina
void add_psuedo_pcb(uint32_t pid, uint32_t tid); 


/*          MUERTE            */
void free_lista_tablas();
void free_tabla_paginas(uint32_t); // le mando el pid, borra la tabla y todo lo asociado a la misma
void remove_from_bitmap(uint32_t); //saca proceso del bitmap del frames
void remove_from_lista_procesos(uint32_t); //saca proceso de lista de procesos cargados en memoria


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          AUXILIARES            */
uint32_t get_tid(uint32_t); //dado un pid devuelvo el tid asociado

/*
t_tabla_paginas get_tabla_from_tid(uint32_t);
t_frame* get_frame_bitmap_from_pid(uint32_t);
t_pseudo_pcb* get_pseudo_pcb_from_pid(uint32_t);
*/

void* get_element_from_pid (t_list*, uint32_t);  //la abstraccion de todas las de arriba, get un elemento de una tabla por el pid


//cosas SUS
bool pagina_valida(t_pagina); 
bool pagina_presente(t_pagina); //consulta pagina cargada en memoria
t_frame* consulta_marco(t_pagina); //entra pagina sale puntero al marco??
int buscar_marco_libre();
void remover_y_eliminar_elementos_de_lista(t_list* lista_original);