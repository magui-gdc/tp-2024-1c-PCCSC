#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "paginacion.h"
#include "logs.h"


void* memoria;   // espacio de memoria real para usuario => tiene que ser un void* PORQUE TODO SE DEBE GUARDAR DE FORMA CONTIGUA
t_list* lista_pcb_tablas_paginas; // lista de procesos con sus respectivas tablas de páginas
t_bitarray *bitmap_marcos_libres; // array dinámico donde su length = cantidad de marcos totales en memoria, que guarda por posición valores 0: marco libre; y 1: marco ocupado por algún proceso
config_struct config;

/*          NACIMIENTO            */

void init_memoria(){
    size_t cantidad_marcos_totales = config.tam_memoria / config.tam_pagina;

    memoria = (void*)calloc(cantidad_marcos_totales, config.tam_pagina);
    if (!memoria) {
        log_error(logger,"Error al inicializar la memoria");
        exit(EXIT_FAILURE);
    }

    //init_bitmap_marcos(cantidad_marcos_totales);
    bitarray_create(bitmap_marcos_libres, cantidad_marcos_totales);

    lista_pcb_tablas_paginas = list_create();
    if (!lista_pcb_tablas_paginas) {
        log_error(logger, "Error al crear la lista de PCB y tablas de páginas");
        free(memoria);
        exit(EXIT_FAILURE);
    }
}

/*
void init_bitmap_marcos(size_t cantidad_marcos_totales){
    size_t tamanio_bitmap = cantidad_marcos_totales / 8; // porque necesitamos el valor en BYTES
    if (cantidad_marcos_totales % 8 != 0) {
        tamanio_bitmap++;  // se anañe un byte extra si hay bits adicionales parciales que no llegarían a completar el byte
    }
    char *bitmap = malloc(tamanio_bitmap); // tamanio en bytes que se va a reservar
    if (!bitmap) {
        log_error(logger,"Error en la creación del Bitmap de Marcos Libres");
        exit(EXIT_FAILURE);
    }

    bitmap_marcos_libres = bitarray_create(bitmap, tamanio_bitmap);
    if (!bitmap_marcos_libres) {
        log_error(logger, "Error al crear el bitarray de marcos libres");
        free(bitmap);
        exit(EXIT_FAILURE);
    }

    // inicializa todos los bits a 0 (todos los marcos están libres)
    for (size_t i = 0; i < cantidad_marcos_totales; i++) {
        bitarray_clean_bit(bitmap_marcos_libres, i);
    }
}
*/

void crear_proceso(uint32_t pid, char* path, uint32_t length_path) {
    t_pcb* proceso_memoria = malloc(sizeof(t_pcb));
    if (!proceso_memoria){
        log_error(logger, "Error en la asignación de memoria para proceso en memoria.");
        exit(EXIT_FAILURE);
    }

    proceso_memoria->pid = pid;
    proceso_memoria->path_proceso = malloc(length_path + 1);
    if (!proceso_memoria->path_proceso) {
        log_error(logger, "Error en la asignación de memoria para path del proceso.");
        free(proceso_memoria);
        exit(EXIT_FAILURE);
    }

    strcpy(proceso_memoria->path_proceso, path);
    
    proceso_memoria->tabla_paginas = list_create();
    if (!proceso_memoria->tabla_paginas) {
        log_error(logger, "Error en la creación de la lista de páginas del proceso.");
        free(proceso_memoria->path_proceso);
        free(proceso_memoria);
        exit(EXIT_FAILURE);
    }

    proceso_memoria->cant_paginas = 0;

    list_add(lista_pcb_tablas_paginas, proceso_memoria);
    log_creacion_destruccion_de_tabla_de_pagina(logger, pid, 0);
}


void create_pagina(t_list *tabla_paginas){

    t_pagina *pagina_aux = malloc(sizeof(t_pagina));
    if (!pagina_aux){
        log_error(logger, "Error en la asignacion de memoria para una pagina.");
        exit(EXIT_FAILURE);
    }

    pagina_aux->marco = -1; 
    pagina_aux->offset = 0;

     if (list_add(tabla_paginas, pagina_aux) == -1) {
        log_error(logger, "Error en la inserción de una Página en la Tabla de Páginas.");
        free(pagina_aux);  
        exit(EXIT_FAILURE);
    }

    // tabla_paginas->cant_paginas++; tabla paginas es de tipo t_list
    
    list_add(tabla_paginas, pagina_aux);
    free(pagina_aux);
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          MUERTE            */

void eliminar_proceso(uint32_t pid){
    // hacer priero el resize, etc. 
    // TODO: recorrer la tabla de páginas, buscar los marcos asociados a cada página y marcarlos como libres
    // TODO: habría que mandar señal a la CPU para que libere TBL???? mmmmmmmm no creo analizar
    // TODO: remover de la lista de procesos el proceso, liberar atributos como el path_instrucciones y la tabla de páginas, liberar por último el proceso completo
    //free_tabla_paginas(pid); 
    //remove_from_bitmap(pid);  
    //remove_from_lista_procesos(pid);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          AUXILIARES            */

t_pcb* get_element_from_pid(uint32_t pid_buscado){ 
    bool comparar_pid(void *elemento){
        t_pcb *pcb = (t_pcb *)elemento;
        return pcb->pid == pid_buscado;
    }
    t_pcb* encontrado = (t_pcb*)list_find(lista_pcb_tablas_paginas, comparar_pid);
    return encontrado; 
}

void remover_y_eliminar_elementos_de_lista(t_list* lista_original){
    t_list_iterator* iterator = list_iterator_create(lista_original);
    while (list_iterator_has_next(iterator)) {
        // Obtener el siguiente elemento de la lista original
        void* element = list_iterator_next(iterator);
        free(element);
    }
    list_iterator_destroy(iterator);
} // TODO: probarlo

int get_bitman_index(void* direccion_marco){
    uintptr_t offset = (uintptr_t)direccion_marco - (uintptr_t)memoria;
    return offset / config.tam_pagina;
}

/*          AUXILIARES PARA RESIZE           */

uint32_t *buscar_marcos_libres(size_t cantidad_marcos_libres) {
    size_t max_bit = bitarray_get_max_bit(bitmap_marcos_libres);
    size_t marcos_libres_encontrados = 0;
    uint32_t *marcos_libres = malloc(cantidad_marcos_libres * sizeof(uint32_t));

    for (size_t i = 0; i < max_bit && marcos_libres_encontrados < cantidad_marcos_libres; i++) {
        if (!bitarray_test_bit(bitmap_marcos_libres, i)) {
            marcos_libres[marcos_libres_encontrados] = i;
            marcos_libres_encontrados++;
        }
    }

    if (marcos_libres_encontrados < cantidad_marcos_libres) {
        // no se encontraron suficientes marcos libres
        free(marcos_libres);
        return NULL;
    }

    return marcos_libres;
}

uint32_t primer_marco_libre() {
    uint32_t *marcos_libres = buscar_marcos_libres(1);
    uint32_t primer_marco_libre = marcos_libres[0];
    free(marcos_libres);
    return primer_marco_libre;
}

bool suficiente_memoria(int memoria_solicitada){
    return memoria_solicitada <= config.tam_memoria; // se puede asignar a la memoria un único proceso
}

/*
bool suficientes_marcos(int cant_paginas_requeridas){
    return cant_paginas_requeridas <= cant_marcos_libres();
}
*/ // ahora está la función buscar_marcos_libres, con preguntar que eso no traiga NULL ya estaría. 

int cant_paginas_proceso(uint32_t pid){
    t_tabla_paginas* tabla_proceso = get_tabla_from_pid(pid);
    return tablaproceso->cant_paginas;
}
