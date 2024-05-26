#include <utils/utilsCliente.h>
#include <utils/utilsServer.h>
#include <commons/config.h>
#include <commons/string.h>
#include <utils/buffer.h>

// ---------- CONSTANTES ---------- //
const char *estado_proceso_strings[] = {
    "NEW", 
    "READY", 
    "RUNNING", 
    "BLOCKED", 
    "EXIT"
};


// --------- DECLARACION ESTRUCTURAS KERNEL --------- //
typedef void (*fc_puntero)(); // PUNTERO A FUNCION
typedef enum {
    NEW, 
    READY, 
    RUNNING, 
    BLOCKED, 
    EXIT
} e_estado_proceso; // ESTADOS PROCESOS

typedef struct{
    char* puerto_escucha;
    char* ip_memoria;
    char* puerto_memoria;
    char* ip_cpu;
    char* puerto_cpu_dispatch;
    char* puerto_cpu_interrupt;
    char* algoritmo_planificacion;
    int quantum;
} config_struct; // CONFIGURACIONES ESTÁTICAS KERNEL

typedef struct {
    uint32_t pid;
    uint32_t program_counter;
    uint8_t quantum;
    t_registros_cpu registros;
    e_estado_proceso estado;
} t_pcb; // PCB

typedef struct {
    uint8_t FLAG_FINALIZACION;
    uint32_t PID;
} prcs_fin;

// --------- FUNCIONES DE CONFIGURACION --------- //
void cargar_config_struct_KERNEL(t_config*);

// --------- FUNCIONES PARA HILOS PRINCIPALES --------- //
void* consola_kernel(void*);
void* planificar_corto_plazo(void*);
void* planificar_largo_plazo(void*);
void* planificar_new_to_ready(void*);
void* planificar_all_to_exit(void*);

// ---------  --------- //
void iniciar_proceso(char*);
void consola_interactiva(char*);
void enviar_proceso_a_cpu();
void recupera_contexto_proceso(t_sbuffer* buffer);

// --------- FUNCIONES ALGORITMOS DE PLANIFICACION --------- //
fc_puntero obtener_algoritmo_planificacion();
void algortimo_fifo();
void algoritmo_rr();
void algoritmo_vrr();
void* control_quantum(void*);