#include <utils/utilsServer.h>

/*
PUERTO_ESCUCHA=8002
TAM_MEMORIA=4096                    este es el tamaño de la memoria fisica
TAM_PAGINA=32                       tamaño de cada pagina,
PATH_INSTRUCCIONES=/home/utnso/scripts-pruebas
RETARDO_RESPUESTA=1000
*/

void crear_proceso(uint32_t,char*);
void eliminar_proceso(uint32_t);
void aplicar_retardo();


//  FUNCIONES ACCESO A ESPACIO USUARIO
/*
    LEER MEMORIA
    devolver el valor que se encuentra a partir de la dirección física pedida.
*/

char *leer_memoria(int, int, uint32_t);