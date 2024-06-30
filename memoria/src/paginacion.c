#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "paginacion.h"
#include "logs.h"


void* memoria;   // espacio de memoria real para usuario => tiene que ser un void* PORQUE TODO SE DEBE GUARDAR DE FORMA CONTIGUA
t_list* lista_pcb_tablas_paginas; // lista de procesos con sus respectivas tablas de páginas
t_bitarray *bitmap_marcos; // array dinámico donde su length = cantidad de marcos totales en memoria, que guarda por posición valores 0: marco libre; y 1: marco ocupado por algún proceso
size_t cantidad_marcos_totales;
config_struct config;

/*          NACIMIENTO            */

void init_memoria(){
    cantidad_marcos_totales = config.tam_memoria / config.tam_pagina;

    memoria = (void*)calloc(cantidad_marcos_totales, config.tam_pagina);
    if (!memoria) {
        log_error(logger,"Error al inicializar la memoria");
        exit(EXIT_FAILURE);
    }

    init_bitmap_marcos();
    //bitarray_create(bitmap_marcos, cantidad_marcos_totales); el segundo parámetro del create es un char* (podría ser un void*), y bitarrat_create devuelve el t_bitarray cargado!

    lista_pcb_tablas_paginas = list_create();
    // el malloc se hace dentro de list_create, si falla el malloc falla la función y se anuncia en pantalla, el chequeo del malloc no siempre es necesario y nos ahorramos código así
}


void init_bitmap_marcos(){
    size_t tamanio_bitmap = cantidad_marcos_totales / 8; // porque necesitamos el valor en BYTES
    if (cantidad_marcos_totales % 8 != 0) {
        tamanio_bitmap++;  // se anañe un byte extra si hay bits adicionales parciales que no llegarían a completar el byte
    }
    char *bitmap = malloc(tamanio_bitmap); // tamanio en bytes que se va a reservar
    if (!bitmap) {
        log_error(logger,"Error en la creación del Bitmap de Marcos Libres");
        exit(EXIT_FAILURE);
    }

    bitmap_marcos = bitarray_create_with_mode(bitmap, tamanio_bitmap, LSB_FIRST);

    // inicializa todos los bits a 0 (todos los marcos están libres)
    for (size_t i = 0; i < cantidad_marcos_totales; i++) {
        bitarray_clean_bit(bitmap_marcos, i);
    }
}


void crear_proceso(uint32_t pid, char* path, uint32_t length_path) {
    t_pcb* proceso_memoria = malloc(sizeof(t_pcb));
    if (!proceso_memoria){
        log_error(logger, "Error en la asignación de memoria para proceso en memoria.");
        exit(EXIT_FAILURE);
    }

    proceso_memoria->pid = pid;
    proceso_memoria->path_proceso = malloc(length_path + 1);
    strcpy(proceso_memoria->path_proceso, path);
    proceso_memoria->tabla_paginas = list_create();

    list_add(lista_pcb_tablas_paginas, proceso_memoria);
    log_creacion_destruccion_de_tabla_de_pagina(logger, pid, 0);
}


void create_pagina(t_list *tabla_paginas, uint32_t marco){

    t_pagina *pagina_aux = malloc(sizeof(t_pagina));
    if (!pagina_aux){
        log_error(logger, "Error en la asignacion de memoria para una pagina.");
        exit(EXIT_FAILURE);
    }

    pagina_aux->id_marco = marco; // se le pasa el marco que va a tener asignada la página cuando se la crea
    pagina_aux->offset = 0;
    list_add(tabla_paginas, pagina_aux);
    
    //free(pagina_aux); // NO se libera acá! se libera una vez que se saca la página de la lista, sino ya no existiría el puntero recién creado en la lista de tabla_paginas!
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          MUERTE            */

void liberar_paginas(t_pcb* proceso_a_liberar, int cantidad_paginas_a_liberar){
    int cantidad_paginas_ocupadas = list_size(proceso_a_liberar->tabla_paginas);
    int inicio = cantidad_paginas_ocupadas - 1;
    int fin = cantidad_paginas_ocupadas - cantidad_paginas_a_liberar; // cantidad_paginas_a_liberar es igual a cantidad_paginas_ocupadas cuando se está eliminando el proceso!
    for(int i = inicio; i >= fin; i--){       
        // REMUEVE LA PAGINA DE LA TABLA DEL PROCESO
        t_pagina* pagina_liberada = (t_pagina*) list_remove(proceso_a_liberar->tabla_paginas, i);

        // LIBERA MARCO DEL BITMAP
        bitarray_clean_bit(bitmap_marcos, pagina_liberada->id_marco); 
        // No se borran los datos del marco

        log_debug(logger, "libere la pagina de la posicion %d de la tabla de pags. del proceso %u que guardaba el marco %u", i, proceso_a_liberar->pid,  pagina_liberada->id_marco);

        // LIBERA/ELIMINA página
        free(pagina_liberada);      
    }
}

void eliminar_proceso(uint32_t pid){
    
    // Función de condición para buscar el proceso por PID
    bool buscar_por_pid(void* data) {
        return ((t_pcb*)data)->pid == pid; // compara con el pid pasado por parámetro a la función eliminar_proceso
    };

    // 1. Busca y elimina el proceso de la lista de procesos en memoria
    t_pcb* proceso_a_eliminar = (t_pcb*)list_remove_by_condition(lista_pcb_tablas_paginas, buscar_por_pid);

    // 2. Recorre la tabla de páginas, buscar los marcos asociados a cada una, los marca como libres y libera las páginas. 
    int tamanio_tabla_pagina = list_size(proceso_a_eliminar->tabla_paginas);
    liberar_paginas(proceso_a_eliminar, tamanio_tabla_pagina); 
    log_creacion_destruccion_de_tabla_de_pagina(logger, proceso_a_eliminar->pid, tamanio_tabla_pagina);

    // 3. Libero el resto de la memoria
    list_destroy(proceso_a_eliminar->tabla_paginas);
    free(proceso_a_eliminar->path_proceso);
    free(proceso_a_eliminar);
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

uint32_t obtener_marco_proceso(uint32_t proceso, int pagina){
    t_pcb* proceso_tabla = get_element_from_pid(proceso);
    t_pagina* pagina_tabla = list_get(proceso_tabla->tabla_paginas, pagina);
    log_acceso_a_tabla_de_pagina(logger, proceso, pagina, pagina_tabla->id_marco);
    // todo: que pasa si retorna NULL?? creo que en un issue vi que no van a pedir datos de paginas no existentes en un proceso
    return pagina_tabla->id_marco;
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
    size_t max_bit = bitarray_get_max_bit(bitmap_marcos);
    size_t marcos_libres_encontrados = 0;
    uint32_t *marcos_libres = malloc(cantidad_marcos_libres * sizeof(uint32_t));

    for (size_t i = 0; i < max_bit && marcos_libres_encontrados < cantidad_marcos_libres; i++) {
        if (!bitarray_test_bit(bitmap_marcos, i)) {
            marcos_libres[marcos_libres_encontrados] = i; // [7, 18, 30]
            marcos_libres_encontrados++;
        }
    }

    if (marcos_libres_encontrados < cantidad_marcos_libres) {
        // no se encontraron suficientes marcos libres
        free(marcos_libres);
        return NULL;
    }

    // los marco como ocupados 
    for (size_t i = 0; i < cantidad_marcos_libres; i++) {
        bitarray_set_bit(bitmap_marcos, marcos_libres[i]);
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

int cant_paginas_ocupadas_proceso(uint32_t pid){
    t_pcb* proceso = get_element_from_pid(pid);
    return list_size(proceso->tabla_paginas);
}
