#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "paginacion.h"

/*          NACIMIENTO            */

t_lista_tablas* init_lista_tablas(){

    lista_tablas = malloc(t_lista_tablas); //asigno espacio para la estructura que maneja las tablas de pag
    if(!lista_tablas){
        log_error(logger, "Error en la creacion de la Lista de Tablas de Pagina");
        exit(EXIT_FAILURE);
    }    

    lista_tablas.tabla_paginas = list_create(); //creo la lista de las tablas de pagina
    lista_tablas.cant_tablas = 0; // es esto realmente necesario? nevermind

    return &lista_tablas;
}

t_tabla_paginas* create_tabla_paginas(u_int32_t pid){

    t_tabla_paginas tabla_aux;
    int posicion;

    tabla_aux.pid = pid;
    tabla_aux.tid = tid; tid++;
    tabla_aux.paginas = list_create();
    tabla_aux.cant_paginas = 0;

    posicion = list_add(lista_tablas.tabla_paginas,tabla_aux); 

    if(!(list_get(lista_tablas.tabla_paginas,posicion))){
        log_error(logger, "Error en la insercion de una Tabla en la Lista de Tablas de Pagina");
        exit(EXIT_FAILURE);
    }

    return list_get(lista_tablas.tabla_paginas,posicion); //esto esta bien?? xd

}

