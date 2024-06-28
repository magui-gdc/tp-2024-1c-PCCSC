#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/buffer.h>
#include <commons/bitarray.h>

typedef struct {
    char* puerto_escucha;
    int tam_memoria;
    int tam_pagina;
    char* path_instrucciones;
    int retardo_respuesta;
} config_struct;
typedef struct{   
    uint32_t id_marco;   // numero de marco asociado a la página
    uint32_t offset;  // checkear tipo de dato
    // bool presence; // no tenemos memoria virtual, creo que no es necesario esto
} t_pagina;

typedef struct{
    uint32_t pid;  // pid proceso en actual ejecución
    char* path_proceso; // path de las instrucciones
    t_list* tabla_paginas; // tabla de páginas asociada al proceso (los elementos de la lista serán de tipo t_pagina*)
    int cant_paginas; // cantidad de paginas ocupadas actualmente por dicho proceso
} t_pcb; 

/*
typedef struct{ // SIEMPRE CARGADO EN MEMORIA
    t_list* tabla_paginas;
    int cant_tablas; // esto se puede tener con un list_size
} t_lista_tablas;
*/ // esto se puede guardar directamente en el pcb de cada proceso en memoria (t_pcb)


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifndef PAGINACION_H
#define PAGINACION_H

extern void* memoria;   // espacio de memoria real para usuario => tiene que ser un void* PORQUE TODO SE DEBE GUARDAR DE FORMA CONTIGUA
extern t_list* lista_pcb_tablas_paginas; // lista de procesos con sus respectivas tablas de páginas
extern t_bitarray *bitmap_marcos_libres; // array dinámico donde su length = cantidad de marcos totales en memoria, que guarda por posición valores 0: marco libre; y 1: marco ocupado por algún proceso

extern config_struct config;
#endif

//extern t_lista_tablas *lista_tablas;
//extern uint32_t tid; //contador para el TABLE IDENTIFICATOR jaja
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


/*          NACIMIENTO            */
void cargar_config_struct_MEMORIA(t_config*);
void init_memoria();
void init_paginacion();

// void init_lista_tablas(); //inicializa la lista de tablas y devuelve el puntero
//void init_bitmap_marcos(size_t); deprecated ahre ahora usamos bitarray de las commons
void crear_proceso(uint32_t pid, char* path, uint32_t length_path); // inserta nuevo pcb junto a su tabla de páginas inicializada
void eliminar_proceso(uint32_t); // libera proceso de memoria
void create_pagina(t_list*); //le mandas la lista de la pagina e inserta la nueva pagina
void add_psuedo_pcb(uint32_t pid, uint32_t tid); 


/*          MUERTE            */
void free_lista_tablas();
void free_tabla_paginas(uint32_t); // le mando el pid, borra la tabla y todo lo asociado a la misma
void remove_from_bitmap(uint32_t); //saca proceso del bitmap del marcos
void remove_from_lista_procesos(uint32_t); //saca proceso de lista de procesos cargados en memoria


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          AUXILIARES            */
uint32_t get_tid(uint32_t); //dado un pid devuelvo el tid asociado

t_pcb* get_element_from_pid (uint32_t);

//cosas SUS
bool pagina_valida(t_pagina); 
bool pagina_presente(t_pagina); //consulta pagina cargada en memoria
// t_marco* consulta_marco(t_pagina); //entra pagina sale puntero al marco??
int buscar_marco_libre();
void remover_y_eliminar_elementos_de_lista(t_list* lista_original);

/*          AUXILIARES PARA RESIZE           */

uint32_t* buscar_marcos_libres(size_t cantidad_marcos_libres);
uint32_t primer_marco_libre();
t_list* lista_marcos_libres();

bool suficiente_memoria(int);
bool suficientes_marcos(int);