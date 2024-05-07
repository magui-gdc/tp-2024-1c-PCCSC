#include <utils/utilsServer.h>
#include <utils/utilsCliente.h>
#include <commons/config.h>
#include <utils/config.h>

/*
IP_MEMORIA=127.0.0.1
PUERTO_MEMORIA=8002
PUERTO_ESCUCHA_DISPATCH=8006
PUERTO_ESCUCHA_INTERRUPT=8007
CANTIDAD_ENTRADAS_TLB=32
ALGORITMO_TLB=FIFO
*/

typedef struct
{
    char* ip_memoria;
    char* puerto_memoria;
    char* puerto_escucha_dispatch; 
    char* puerto_escucha_interrupt; 
    char* cantidad_entradas_tlb;
    char* algoritmo_tlb;
} config_struct;

extern config_struct config;

void cargar_config_struct_CPU(t_config*);

void inicializar_registros();

void* recibir_interrupcion(void*);