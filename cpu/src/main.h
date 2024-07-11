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
    int cantidad_entradas_tlb;
    char* algoritmo_tlb;
} config_struct;

typedef struct {
    uint32_t pid;
    op_code motivo_interrupcion;
    uint8_t bit_prioridad;
} t_pic;

typedef struct {
    uint32_t pid;
    int pagina;
    uint32_t marco;
    uint64_t timestamp; // Timestamp para registrar el tiempo de acceso con precisión en nanosegundos PARA LRU
} t_tlb;

extern config_struct config;
extern t_list* tlb_list;

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

// cálculos dirección lógica - física para mandar a ejecutar en las instrucciones que corresponda
// devuelve un buffer cargado con la/s peticione/s a memoria para leer/escribir con los datos ya procesados (página/marco + offset + tamanio a leer/escribir + dato a escribir)
// consulta además en el TLB
t_sbuffer* mmu(const char* operacion, uint32_t direccion_logica, uint32_t bytes_peticion, uint32_t dato_escribir); // ver si después se pasa en algún momento char* desde acá!!

// manda socket PEDIR_MARCO a memoria y retorna el número de marco encontrado para el proceso y su pagina
uint32_t solicitar_marco_a_memoria(uint32_t proceso, int pagina);
// buscar en TLB por PID + página
t_tlb* buscar_marco_tlb(uint32_t proceso, uint32_t nro_pagina);
// agrega el marco según el algoritmo para la TLB
t_tlb* agregar_marco_tlb(uint32_t proceso, int pagina, uint32_t marco); 

// LRU / FIFO
uint64_t obtener_timestamp(); // devuelve microsegundos actuales
void remover_entrada_segun_algoritmo();
