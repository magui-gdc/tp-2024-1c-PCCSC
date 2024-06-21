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

//  AJUSTAR TAMANIO DE PROCESO
/*
    Ampliación de un proceso
    Se deberá ampliar el tamaño del proceso al final del mismo, pudiendo solicitarse múltiples páginas. 
    Es posible que en un punto no se puedan solicitar más marcos ya que la memoria se encuentra llena, 
    por lo que en ese caso se deberá contestar con un error de Out Of Memory.

*/

void ampliar_proceso(uint32_t);

