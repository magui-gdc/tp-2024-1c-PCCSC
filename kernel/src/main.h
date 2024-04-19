#include <utils/utilsCliente.h>
#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/config.h>

typedef struct
{
    char* puerto_escucha;
    char* ip_memoria;
    char* puerto_memoria;
    char* ip_cpu;
    char* puerto_cpu_dispatch;
    char* puerto_cpu_interrupt;
    char* algoritmo_planificacion;
    char** recursos;
    char** instancias; 
    int grado_multiprogramacion
} config_struct;

extern config_struct config;

void cargar_config_struct_KERNEL(t_config*);
void* consola_kernel(void*);
void* planificar_corto_plazo(void*);
void* planificar_largo_plazo(void*);
