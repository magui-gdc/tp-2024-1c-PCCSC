#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "paginacion.h"
#include "logs.h"


// PARA QUE LAS VARIABLES EXISTAN EN SUS ARCHIVOS C, ADEMÁS DEL EXTERN SE DEBEN LLAMAR DESDE ACÁ!

void* memoria;   // espacio de memoria real para usuario => tiene que ser un void* PORQUE TODO SE DEBE GUARDAR DE FORMA CONTIGUA
t_list* lista_pcb_tablas_paginas; // lista de procesos con sus respectivas tablas de páginas
uint8_t* bitmap_frames_libres; // array dinámico donde su length = cantidad de frames/marcos totales en memoria, que guarda por posición valores 0: marco libre; y 1: marco ocupado por algún proceso
int cantidad_frames_totales;
config_struct config;

/*          NACIMIENTO            */

void init_memoria(){
    cantidad_frames_totales = config.tam_memoria / config.tam_pagina;
    memoria = (void*)calloc(cantidad_frames_totales, config.tam_pagina);
    // init_lista_tablas(); // no es necesario, se guarda la tabla de procesos en el t_pcb
    init_bitmap_frames();
    init_lista_procesos();
}

/*void init_lista_tablas(){
    lista_tablas = malloc(sizeof(t_lista_tablas)); // asigno espacio para la estructura que maneja las tablas de pag
    if (!lista_tablas){
        log_error(logger, "Error en la creacion de la Lista de Tablas de Pagina.");
        exit(EXIT_FAILURE);
    }
    lista_tablas->tabla_paginas = list_create(); // creo la lista de las tablas de pagina
    lista_tablas->cant_tablas = 0;               // es esto realmente necesario? nevermind
} */

void init_bitmap_frames(){

    bitmap_frames_libres = malloc(cantidad_frames_totales * sizeof(uint8_t)); 

    if (!bitmap_frames_libres){
        log_error(logger, "Error en la creacion del Bitmap de Frames Libres.");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < cantidad_frames_totales; i++) {
        bitmap_frames_libres[i] = 0; // inicializa todos los marcos como 'libres'
    }

    /*bitmap_frames_libres = list_create();

    int cant_frames = config.tam_memoria / config.tam_pagina;

    t_frame *frame_aux = malloc(sizeof(t_frame));
    frame_aux->pagina = -1;
    frame_aux->pid = -1;
    frame_aux->presence = false;

    
    for (int i = 0; i < cant_frames; i++){ // creo la lista con los frames que hay disponibles todos inicializados "vacios"
        list_add(bitmap_frames_libres, frame_aux);
    }*/
}

void init_lista_procesos(){
    lista_pcb_tablas_paginas = list_create();
    if (!lista_pcb_tablas_paginas){
        log_error(logger, "Error en la creacion de la Lista de Procesos - Tabla de Página por proceso.");
        exit(EXIT_FAILURE);
    }
}


/* void create_tabla_paginas(uint32_t pid, ){

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
} */ // se reemplaza por crear_proceso 

void crear_proceso(uint32_t pid, char* path, uint32_t length_path) {
    t_pcb* proceso_memoria = malloc(sizeof(proceso_memoria));
    if (!proceso_memoria){
        log_error(logger, "Error en la asignación de memoria para proceso en memoria.");
        exit(EXIT_FAILURE);
    }

    proceso_memoria->pid = pid;
    proceso_memoria->path_proceso = malloc(length_path + 1);
    strcpy(proceso_memoria->path_proceso, path);
    proceso_memoria->tabla_paginas = list_create();
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

    // pagina_aux->frame = -1; no es de time frame el -1
    pagina_aux->offset = 0;

    if (list_add(tabla_paginas, pagina_aux) == -1){
        log_error(logger, "Error en la insercion de una Pagina en la Tabla de Paginas.");
        exit(EXIT_FAILURE);
    }

    // tabla_paginas->cant_paginas++; tabla paginas es de tipo t_list
    
    list_add(tabla_paginas, pagina_aux);
    free(pagina_aux);
}

/*void add_psuedo_pcb(uint32_t pid, uint32_t tid){

    t_pseudo_pcb *pcb_aux = malloc(sizeof(t_pseudo_pcb));
    pcb_aux->pid = pid;
    pcb_aux->tid = tid;

    list_add(lista_procesos, pcb_aux);
    free(pcb_aux);
} */

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          MUERTE            */

/*
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
*/

void eliminar_proceso(uint32_t pid){
    // TODO: recorrer la tabla de páginas, buscar los marcos asociados a cada página y marcarlos como libres
    // TODO: habría que mandar señal a la CPU para que libere TBL???? mmmmmmmm no creo analizar
    // TODO: remover de la lista de procesos el proceso, liberar atributos como el path_instrucciones y la tabla de páginas, liberar por último el proceso completo
    //free_tabla_paginas(pid); 
    //remove_from_bitmap(pid);  
    //remove_from_lista_procesos(pid);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          AUXILIARES            */

/* uint32_t get_tid(uint32_t pid){
    bool comparar_pid(void *elemento){
        t_tabla_paginas *tabla = (t_tabla_paginas *)elemento;
        return tabla->pid == pid;
    }
    t_tabla_paginas* tabla_encontrada = (t_tabla_paginas*)list_find(lista_tablas, comparar_pid);

    return tabla_encontrada->tid; // si no funciona pasarlo como puntero a funcion
} */ // ya no hay TID por tabla de páginas

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

/*          AUXILIARES PARA RESIZE           */

int cant_frames_libres(){
    int contador = 0;
    for (int i = 0; i < cantidad_frames_totales; i++){
        if(bitmap_frames_libres[i] == 0)
            contador++;
    }
    return contador; // list_size(lista_frames_libres());   
}

/*
t_list* lista_frames_libres(){
    bool frame_libre(void* frame_libre){    // esta la declaramos en el .h???
        if(frame->presence){
            return true;
        }
        else return false;
    }
    return list_filter(bitmap_frames_libres, bool(*frame_libre)(void *));
} */ // queda obsoleto porque bitmap es un array dinámico de enteros entre 0-1 únicamente, no hay necesidad de lista


bool suficiente_memoria(int memoria_solicitada){
    return memoria_solicitada <= config.tam_memoria; // se puede asignar a la memoria un único proceso
}

bool suficientes_frames(int cant_paginas_requeridas){
    return cant_paginas_requeridas <= cant_frames_libres();
}