#include <utils/utilsCliente.h>
#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/config.h>
#include <commons/string.h>

typedef enum {
    NEW, 
    READY, 
    RUNNING, 
    BLOCKED, 
    TERMINATED
} e_estado_proceso;

typedef struct
{
    char* puerto_escucha;
    char* ip_memoria;
    char* puerto_memoria;
    char* ip_cpu;
    char* puerto_cpu_dispatch;
    char* puerto_cpu_interrupt;
    char* algoritmo_planificacion;
    int quantum;
    //char** recursos;
    //char** instancias; 
    //int grado_multiprogramacion;
} config_struct;

typedef struct {
    uint32_t pid;
    uint32_t program_counter;
    uint8_t quantum;
    t_registros_cpu registros;
    e_estado_proceso estado;
} t_pcb;

extern config_struct config;
extern t_list* pcb_list; // lista dinámica que contiene los PCB de los procesos creados
extern uint32_t pid; // contador para determinar el PID de cada proceso creado


void cargar_config_struct_KERNEL(t_config*);
void* consola_kernel(void*);
void* planificar_corto_plazo(void*);
void* planificar_largo_plazo(void*);
