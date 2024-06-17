
#include <utils/utilsCliente.h>
#include <utils/utilsServer.h>
#include <commons/config.h>
#include <commons/string.h>
#include <utils/buffer.h>

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
    DESALOJADO_POR_IO, 
    PEDIDO_FINALIZACION
} cod_desalojo; 
// FINALIZAR_PROCESO ya está en op_code que es la estructura que se carga en el t_pic de cpu (para que CPU y KERNEL compartan mismos códigos de operación)

typedef struct {
    char* nombre_interfaz;
    char* tipo_interfaz;
    int socket_interfaz;
    int disponibilidad; // 0: si esta libre; 1: si está siendo usado por otro proceso
    t_mqueue* cola_bloqueados;
} t_interfaz;

typedef struct{
    char* puerto_escucha;
    char* ip_memoria;
    char* puerto_memoria;
    char* ip_cpu;
    char* puerto_cpu_dispatch;
    char* puerto_cpu_interrupt;
    char* algoritmo_planificacion;
    char** recursos;
    int quantum;
} config_struct; // CONFIGURACIONES ESTÁTICAS KERNEL

typedef struct {
    uint32_t pid;
    op_code motivo_interrupcion;
    uint8_t bit_prioridad;
} t_pic;

typedef struct {
    uint32_t pid;
    e_estado_proceso estado;
    t_registros_cpu registros;
    uint8_t quantum;
    cod_desalojo desalojo; 
    char path[256];
    int* recursos; // array que guarda las instancias de los recursos del archivo config de kernel, por lo tanto, no hace falta guardar el nombre (el orden de cada elemento del array lo obtenemos a partir del orden del config), sino por (posición del array)=recurso la cantidad de instancias que está utilizando
    t_mqueue* cola_bloqueado; // guardas un puntero de la cola de bloqueados donde está el proceso en estado BLOCKED (recurso / io)
    t_spaquete* instruccion_io; // guarda ya cargado el paquete de la instrucción IO cuando el proceso se bloquea y espera 
} t_pcb; // PCB

typedef struct {
    char algoritmo[4];
    t_pcb* proceso_running;
} control_quantum_params;

typedef struct {
    uint8_t FLAG_FINALIZACION;
    uint32_t PID;
} prcs_fin;

extern t_list* pcb_list;

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
void interpretar_comando_kernel(char*, void*);
void scripts_kernel(char*, void*);
void enviar_proceso_a_cpu();
void recibir_proceso_desalojado();
void recupera_contexto_proceso(t_sbuffer* buffer);
t_pcb* buscar_pcb_por_pid(uint32_t);
t_pcb* extraer_proceso(t_pcb* proceso);
void liberar_recursos(t_pcb* proceso_exit);
void liberar_proceso_en_memoria(uint32_t pid_proceso);
void enviar_interrupcion_a_cpu(t_pic interrupcion);
void manejo_instruccion_io(int instruccion, t_sbuffer* buffer_desalojo, t_pcb* proceso_desalojado);

// --------- FUNCIONES ALGORITMOS DE PLANIFICACION --------- //
fc_puntero obtener_algoritmo_planificacion();
void algoritmo_fifo_rr();
void algoritmo_vrr();
void* control_quantum(void*);
// ejecutar al ppio. para los casos donde se desaloja el proceso en actual ejecución, cualquiera sea el motivo!
void control_quantum_desalojo();
// carga quantum restante en caso de VRR (se debe ejecutar al ppio de cada desalojo por instrucción IO)
void cargar_quantum_restante(t_pcb* proceso_desalojado);

