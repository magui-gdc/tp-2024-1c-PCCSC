#include "MMU.h"
#include <math.h>

extern t_list* lista_TLB; // tamanio TAM_MEMORIA/TAM_PAGINA
extern int TAM_MEMORIA;
extern int TAM_PAGINA;

//TODO: DEFINIR CUANDO SE EJECUTA MMU. CON ESTO, VOY A SABER CUANDO SE DEVUELVE LA RESPUESTA DE MEMORIA DEL FRAME BUSCADO.
void traducir_dlogica_a_dfisica(void* direccion_logica, int conexion_memoria){
    //PEDIR TAMANIO PAGINA A MEMORIA, se hace antes de llamar a la traduccion, o se hace apenas empezar?
    /*int numero_pagina = floor(direccion_logica / TAM_PAGINA);
    int desplazamiento = direccion_logica - numero_pagina * TAM_MEMORIA;


    if(buscar_en_tlb(numero_pagina) != 0){
        //TLB_HIT
        //return nroMarco * tamaño_pagina + desplazamiento
    } else {
        //TLB_MISS
        solicitar_frame_a_memoria(numero_pagina);
        int respuesta = recibir_operacion(conexion_memoria);
        if (respuesta == FRAME_SOLICIDATO){
            //aniadir marco a tlb
            //return nroMarco * tamaño_pagina + desplazamiento
        } else {
            //INTERRUPCION PAGE FAULT
        }
    }*/ 
    // TODO: ERROR DE TIPOS => numero_pagina divide a un void* por un int
}

int solicitar_frame_a_memoria(int numero_pagina, int conexion_memoria){
    // t_sbuffer* buffer_pagina = buffer_create(sizeof(int));
    // TODO: CARGAR BUFFER ANTES DE ENVIAR
    // cargar_paquete(conexion_memoria, PEDIR_FRAME ,buffer_pagina);
    return 0;
}

void agregar_frame_a_tlb(){
    //if tlb llena -> algoritmo de reemplazo
}


int buscar_en_tlb(int pagina){
    return 0;
}
