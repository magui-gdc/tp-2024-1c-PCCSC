#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/buffer.h>
#include <math.h>
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
} t_pagina;

typedef struct{
    uint32_t pid;  // pid proceso en actual ejecución
    char* path_proceso; // path de las instrucciones
    t_list* tabla_paginas; // tabla de páginas asociada al proceso (los elementos de la lista serán de tipo t_pagina*)
} t_pcb; 

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifndef PAGINACION_H
#define PAGINACION_H

extern void* memoria;   // espacio de memoria real para usuario => tiene que ser un void* PORQUE TODO SE DEBE GUARDAR DE FORMA CONTIGUA
extern t_list* lista_pcb_tablas_paginas; // lista de procesos con sus respectivas tablas de páginas
extern t_bitarray *bitmap_marcos; // array dinámico donde su length = cantidad de marcos totales en memoria, que guarda por posición valores 0: marco libre; y 1: marco ocupado por algún proceso
extern size_t cantidad_marcos_totales;
extern config_struct config;
#endif
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


/*          NACIMIENTO            */
void cargar_config_struct_MEMORIA(t_config*);
void init_memoria();
void init_bitmap_marcos();
void crear_proceso(uint32_t pid, char* path, uint32_t length_path); // inserta nuevo pcb junto a su tabla de páginas inicializada
void create_pagina(t_list*, uint32_t); // le mandas la lista de la pagina + el número de marco e inserta la nueva pagina

/*          MUERTE            */
// libera proceso de memoria por completo de memoria
void eliminar_proceso(uint32_t); 
// libera una X cantidad de páginas desde el final de la tabla
void liberar_paginas(t_pcb* proceso_a_liberar, int cantidad_paginas_a_liberar);

/*          AUXILIARES            */
// retorna un pcb de la lista de proceso según el pid pasado por parámetro
t_pcb* get_element_from_pid (uint32_t);
// retorna el número de marco de la pagina de un proceso pasado por parámetro
uint32_t obtener_marco_proceso(uint32_t proceso, int pagina);
// según una cantidad de bytes pasados por parámetro retorna TRUE si hay suficiente memoria en general en la RAM para almacenarlo (sin importar si la memoria está libre o no)
bool suficiente_memoria(int);

/*          AUXILIARES PARA RESIZE           */
// devuelve un array dinámico con la cantidad de marcos libres solicitados, ya marcados como ocupados en el bitmap_marcos. Si no encuentra los suficientes marcos libres retorna NULL
uint32_t* buscar_marcos_libres(size_t cantidad_marcos_libres);
// retorna la cantidad de páginas ocupadas haciendo un list_size a la tabla de páginas del proceso
int cant_paginas_ocupadas_proceso(uint32_t pid);

// quedaron en DESUSO las siguientes funciones:
uint32_t primer_marco_libre();
bool pagina_valida(t_pagina); 
bool pagina_presente(t_pagina); //consulta pagina cargada en memoria
void remover_y_eliminar_elementos_de_lista(t_list* lista_original);
void free_tabla_paginas(uint32_t); // le mando el pid, borra la tabla y todo lo asociado a la misma
void remove_from_bitmap(uint32_t); //saca proceso del bitmap del marcos
void remove_from_lista_procesos(uint32_t); //saca proceso de lista de procesos cargados en memoria
int get_bitman_index(void* direccion_marco);