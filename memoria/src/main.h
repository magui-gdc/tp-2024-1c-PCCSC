#include <commons/temporal.h>

/*          RETARDO CONFIG          */
void tiempo_espera_retardo(t_temporal* timer);


/*          ACCESO ESPACIO USUARIO         */
#define ERROR
#define OK
void* leer_memoria(uint32_t dir_fisica, int tam_lectura);
int escribir_memoria(uint32_t dir_fisica, void* dato, int tam_escritura);

/*          RESIZE            */
void resize_proceso(t_temporal* retardo, int socket_cliente, uint32_t pid, int new_size);
void ampliar_proceso(uint32_t pid, int cantidad, uint32_t* marcos_solicitados);
void reducir_proceso(uint32_t pid, int cantidad);


