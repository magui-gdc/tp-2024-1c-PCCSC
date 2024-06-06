#include "MMU.h"

extern t_list* lista_TLB;
//TODO: DEFINIR CUANDO SE EJECUTA MMU. CON ESTO, VOY A SABER CUANDO SE DEVUELVE LA RESPUESTA DE MEMORIA DEL FRAME BUSCADO.
void traducir_dlogica_a_dfisica(void* direccion_logica, t_config* archivo_config){
    //PEDIR TAMANIO PAGINA A MEMORIA, se hace antes de llamar a la traduccion, o se hace apenas empezar?
    //int numero_pagina = floor(direccion_logica / tamanio_pagina);
    int numero_pagina;
    int desplazamiento;
    //int desplazamiento = direccion_logica - numero_pagina * tamanio_pagina;


    if(buscar_en_tlb(numero_pagina) != 0){
        //TLB_HIT
        //return nroMarco * tamaño_pagina + desplazamiento
    } else {
        //TLB_MISS
        solicitar_frame_a_memoria();
    }
}

void agregar_frame_a_tlb(){
    //if tlb llena -> algoritmo de reemplazo
}



int buscar_en_tlb(int pagina){
    return 0;
}
