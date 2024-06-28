#include <utils/utilsServer.h>

/*
PUERTO_ESCUCHA=8002
TAM_MEMORIA=4096                    este es el tamaño de la memoria fisica
TAM_PAGINA=32                       tamaño de cada pagina,
PATH_INSTRUCCIONES=/home/utnso/scripts-pruebas
RETARDO_RESPUESTA=1000
*/

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          ACCESO ESPACIO USUARIO            */

#define ERROR
#define OK
void* leer_memoria(void* dir_fisica, int tam_lectura);
int escribir_memoria(char* dir_fisica, void* dato);


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/*          RESIZE            */

void ampliar_proceso(uint32_t pid, int new_size);

