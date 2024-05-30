#include <utils/utilsServer.h>
#include <utils/utilsCliente.h>
#include <commons/config.h>
#include <utils/buffer.h>
#include <semaphore.h>

typedef struct{
    char* ip_memoria;
    char* puerto_memoria;
    char* puerto_escucha_dispatch; 
    char* puerto_escucha_interrupt; 
    char* cantidad_entradas_tlb;
    char* algoritmo_tlb;
} config_struct;

typedef struct {
    uint32_t pid;
    op_code motivo_interrupcion;
    uint8_t bit_prioridad;
} t_pic;

extern config_struct config;

void cargar_config_struct_CPU(t_config*);

void inicializar_registros();

void* recibir_interrupcion(void*);

// ---------------- FCS. CICLO INSTRUCCION ----------------
void ciclo_instruccion(int conexion_kernel);
void ejecutar_instruccion(char* leido, int conexion_kernel);
void desalojo_proceso(t_sbuffer** buffer_contexto_proceso, int conexion_kernel, op_code mensaje_desalojo);
void check_interrupt(uint32_t proceso_pid, int conexion_kernel);

bool coinciden_pid(void* element, uint32_t pid);
bool comparar_prioridad(void* a, void* b);
t_list* filtrar_y_remover_lista(t_list* lista_original, bool(*condicion)(void*, uint32_t), uint32_t pid);