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

typedef enum{
    SIN_DESALOJAR,
    DESALOJADO,
    PEDIDO_FINALIZACION
} cod_desalojo; 
// FINALIZAR_PROCESO ya está en op_code que es la estructura que se carga en el t_pic de cpu (para que CPU y KERNEL compartan mismos códigos de operación)


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

/*
typedef struct{ //LO DEFINO ASI PORQUE NO SABEMOS COMO SON LOS RECURSOS AUN
    char* recursoNombre;
}t_recurso;
*/

typedef struct {
    uint32_t pid;
    op_code motivo_interrupcion;
} t_pic;

typedef struct {
    uint32_t pid;
    uint32_t program_counter;
    uint8_t quantum;
    t_registros_cpu registros;
    e_estado_proceso estado;
    char path[256];
    int recursos[3]; // guarda las instancias de los recursos del archivo config de kernel, por lo tanto, no hace falta guardar el nombre (el orden de cada elemento del array lo obtenemos a partir del orden del config), sino por (posición del array)=recurso la cantidad de instancias que está utilizando
    cod_desalojo desalojo; 
    /*SIN_DESALOJAR: está en CPU cuando el estado es RUNNING, 
    DESALOJADO: volvió de CPU y todavía no se trató su mensaje de desalojo, 
    PEDIDO_FINALIZACION: se ejecutó finaliza_proceso desde consola*/
} t_pcb; // PCB

typedef struct {
    uint8_t FLAG_FINALIZACION;
    uint32_t PID;
} prcs_fin;

// --------- FUNCIONES DE CONFIGURACION --------- //
void cargar_config_struct_KERNEL(t_config*);
void inicializar_registros(t_pcb*);

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
void* buscar_pcb_por_pid(uint32_t);
t_pcb* extraer_proceso(uint32_t pid, e_estado_proceso estado);
void liberar_proceso_en_memoria(uint32_t pid_proceso);
void enviar_interrupcion_a_cpu(t_pic interrupcion);

// --------- FUNCIONES ALGORITMOS DE PLANIFICACION --------- //
fc_puntero obtener_algoritmo_planificacion();
void algortimo_fifo();
void algoritmo_rr();
void algoritmo_vrr();
void* control_quantum(void*);