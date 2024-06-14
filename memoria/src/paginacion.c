#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "paginacion.h"
#include "logs.h"


// PARA QUE LAS VARIABLES EXISTAN EN SUS ARCHIVOS C, ADEMÁS DEL EXTERN SE DEBEN LLAMAR DESDE ACÁ!

void* memoria;   // el "espacio de usario", la "memoria real"
t_lista_tablas *lista_tablas;
u_int32_t tid; //contador para el TABLE IDENTIFICATOR jaja

t_list* bitmap_frames_libres; //"bitmap" de paginas cargadas
t_list* lista_procesos; //lista de procesos con sus respectivas listas de paginas
// hay que poner include main.c y main.h???

/*          NACIMIENTO            */

void init_memoria(){
    memoria = malloc(config.tam_memoria);
    init_lista_tablas();
    void init_bitmap_frames();
    void init_lista_procesos();
}

void init_lista_tablas(){

    lista_tablas = malloc(sizeof(t_lista_tablas)); // asigno espacio para la estructura que maneja las tablas de pag
    if (!lista_tablas){
        log_error(logger, "Error en la creacion de la Lista de Tablas de Pagina.");
        exit(EXIT_FAILURE);
    }

    lista_tablas->tabla_paginas = list_create(); // creo la lista de las tablas de pagina
    lista_tablas->cant_tablas = 0;               // es esto realmente necesario? nevermind
}

void init_bitmap_frames(){

    bitmap_frames_libres = list_create();
    if (!bitmap_frames_libres){
        log_error(logger, "Error en la creacion del Bitmap de Frames Libres.");
        exit(EXIT_FAILURE);
    }

    int cant_frames = config.tam_memoria / config.tam_pagina;

    t_frame *frame_aux = malloc(sizeof(t_frame));
    frame_aux->pagina = -1;
    frame_aux->pid = -1;
    frame_aux->presence = false;

    
    for (int i = 0; i < cant_frames; i++){ // creo la lista con los frames que hay disponibles todos inicializados "vacios"
        list_add(bitmap_frames_libres, frame_aux);
    }
}

void init_lista_procesos(){

    lista_procesos = list_create();
    if (!lista_procesos){
        log_error(logger, "Error en la creacion de la Lista de Procesos Libres.");
        exit(EXIT_FAILURE);
    }

}

uint32_t create_tabla_paginas(uint32_t pid){

    t_tabla_paginas* tabla_aux = malloc(sizeof(t_tabla_paginas));
    if (!tabla_aux){
        log_error(logger, "Error en la asignacion de memoria para una tabla.");
        exit(EXIT_FAILURE);
    }

    tabla_aux->pid = pid;
    tabla_aux->tid = tid;
    tid++;
    tabla_aux->paginas = list_create();
    tabla_aux->cant_paginas = 0;

    if ((list_add(lista_tablas->tabla_paginas, tabla_aux)) == -1){
        log_error(logger, "Error en la insercion de una Tabla en la Lista de Tablas de Pagina. PID: %u", pid);
        exit(EXIT_FAILURE);
    }

    list_add(lista_tablas->tabla_paginas, tabla_aux);
    // log_creacion_destruccion_de_tabla_de_pagina(logger, tabla_aux->pid, tabla_aux->tamanio); TODO_ FIX ERROR COMPILACION EN TABLA_AUX
    free(tabla_aux);
    return(tid-1);    
}

void create_pagina(t_list *tabla_paginas){

    t_pagina *pagina_aux = malloc(sizeof(t_pagina));
    if (!pagina_aux){
        log_error(logger, "Error en la asignacion de memoria para una pagina.");
        exit(EXIT_FAILURE);
    }

    // pagina_aux->frame = -1; no es de time frame el -1
    pagina_aux->offset = 0;
    pagina_aux->presence = false;

    if (list_add(tabla_paginas, pagina_aux) == -1){
        log_error(logger, "Error en la insercion de una Pagina en la Tabla de Paginas.");
        exit(EXIT_FAILURE);
    }

    // tabla_paginas->cant_paginas++; tabla paginas es de tipo t_list
    
    list_add(tabla_paginas, pagina_aux);
    free(pagina_aux);
}

void add_psuedo_pcb(uint32_t pid, uint32_t tid){

    t_pseudo_pcb *pcb_aux = malloc(sizeof(t_pseudo_pcb));
    pcb_aux->pid = pid;
    pcb_aux->tid = tid;

    list_add(lista_procesos, pcb_aux);
    free(pcb_aux);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          MUERTE            */

void free_tabla_paginas(uint32_t pid){
    uint32_t tid = get_tid(pid);
    t_tabla_paginas* tabla = (t_tabla_paginas*)get_element_from_pid(lista_tablas, pid);
    //semaforo wait
    list_remove_element(lista_tablas, tabla);
    //signal
    remover_y_eliminar_elementos_de_lista(tabla->paginas);
    free(tabla);
}

void remove_from_bitmap(uint32_t pid){
    t_frame* frame = (t_frame*)get_element_from_pid(bitmap_frames_libres,pid);
    list_remove_element(bitmap_frames_libres, frame);
    free(frame);
}
void remove_from_lista_procesos(uint32_t pid){
    t_pseudo_pcb* pseudo_pcb = (t_pseudo_pcb*)get_element_from_pid(lista_procesos, pid);
    list_remove_element(lista_procesos, pseudo_pcb);
    free(pseudo_pcb);
    
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          AUXILIARES            */

uint32_t get_tid(uint32_t pid){
    bool comparar_pid(void *elemento){
        t_tabla_paginas *tabla = (t_tabla_paginas *)elemento;
        return tabla->pid == pid;
    }
    t_tabla_paginas* tabla_encontrada = (t_tabla_paginas*)list_find(lista_tablas, comparar_pid);

    return tabla_encontrada->tid; // si no funciona pasarlo como puntero a funcion
}

void* get_element_from_pid(t_list* lista, uint32_t pid_buscado){ //ABSTRACCION!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    /*
    bool comparar_pid(void *elemento){
        void *frame = (void *)elemento;
        return elemento->pid == pid_buscado;
    }
    void* elemento = (void*)list_find(lista, comparar_pid);

    return elemento; // si no funciona pasarlo como puntero a funcion
    */ // TODO: FALTA ACLARAR EL TIPO DE LO QUE DEVUELVO PARA PODER USARLO EN EL COMPARAR_PID: no void*
}

/*
t_tabla_paginas* get_tabla_from_tid(uint32_t tid){
    bool comparar_tid(void *elemento){
        t_tabla_paginas *tabla = (t_tabla_paginas *)elemento;
        return tabla->tid == tid;
    }
    t_tabla_paginas* tabla_encontrada = (t_tabla_paginas*)list_find(lista_tablas, comparar_tid);

    return tabla_encontrada; // si no funciona pasarlo como puntero a funcion
}
t_frame* get_frame_bitmap_from_pid(uint32_t pid_buscado){

    bool comparar_pid(void *elemento){
        t_frame *frame = (t_frame *)elemento;
        return frame->pid == pid_buscado;
    }
    t_frame* frame_encontrado = (t_frame*)list_find(bitmap_frames_libres, comparar_pid);

    return frame_encontrado; // si no funciona pasarlo como puntero a funcion
}
t_pseudo_pcb* get_pseudo_pcb_from_pid(uint32_t pid_buscado){

    bool comparar_pid(void *elemento){
        t_pseudo_pcb *pcb = (t_pseudo_pcb *)elemento;
        return pcb->pid == pid_buscado;
    }
    t_pseudo_pcb* pcb_encontrado = (t_pseudo_pcb*)list_find(lista_procesos, comparar_pid);

    return pcb_encontrado; // si no funciona pasarlo como puntero a funcion
}
*/

void remover_y_eliminar_elementos_de_lista(t_list* lista_original){
    t_list_iterator* iterator = list_iterator_create(lista_original);
    while (list_iterator_has_next(iterator)) {
        // Obtener el siguiente elemento de la lista original
        void* element = list_iterator_next(iterator);
        free(element);
    }
    list_iterator_destroy(iterator);
}