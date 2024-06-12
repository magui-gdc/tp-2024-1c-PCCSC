#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "paginacion.h"
// hay que poner include main.c y main.h???

/*          NACIMIENTO            */

void init_memoria(){
    memoria = malloc(config.tam_memoria);
    lista_tablas = init_lista_tablas();
    void init_bitmap_frames();
    void init_lista_procesos();
}

void init_lista_tablas(){

    lista_tablas = malloc(sizeof(t_lista_tablas)); // asigno espacio para la estructura que maneja las tablas de pag
    if (!lista_tablas){
        log_error(logger, "Error en la creacion de la Lista de Tablas de Pagina.");
        exit(EXIT_FAILURE);
    }

    lista_tablas.tabla_paginas = list_create(); // creo la lista de las tablas de pagina
    lista_tablas.cant_tablas = 0;               // es esto realmente necesario? nevermind
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
    frame_aux->presence = FALSE;

    for (int i = 0; i < cant_frames; i++){ // creo la lista con los frames que hay disponibles todos inicializados "vacios"
        list_add(frame_aux, bitmap_frames_libres);
    }
}

void init_lista_procesos(){

    lista_procesos = list_create();
    if (!lista_procesos){
        log_error(logger, "Error en la creacion de la Lista de Procesos Libres.");
        exit(EXIT_FAILURE);
    }

}

int create_tabla_paginas(u_int32_t pid){

    t_tabla_paginas tabla_aux = malloc(sizeof(t_tabla_paginas));
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
        log_error(logger, "Error en la insercion de una Tabla en la Lista de Tablas de Pagina. PID: %s", pid);
        exit(EXIT_FAILURE);
    }

    return list_add(lista_tablas->tabla_paginas, tabla_aux);
}

int create_pagina(t_list *tabla_paginas){

    t_pagina *pagina_aux = malloc(sizeof(t_pagina));
    if (!pagina_aux){
        log_error(logger, "Error en la asignacion de memoria para una pagina.");
        exit(EXIT_FAILURE);
    }

    pagina_aux->frame = -1;
    pagina_aux->offset = 0;
    pagina_aux->presence = FALSE;

    if (list_add(tabla_paginas, pagina_aux) == -1){
        log_error(logger, "Error en la insercion de una Pagina en la Tabla de Paginas. PID: %s", tabla_paginas->pid);
        exit(EXIT_FAILURE);
    }

    tabla_paginas->cant_paginas++;

    return list_add(tabla_paginas, pagina_aux);
}
