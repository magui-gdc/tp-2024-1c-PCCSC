#include <utils/utilsServer.h>
#include <utils/utilsCliente.h>
#include <commons/config.h>
#include <utils/buffer.h>

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

// ---------------- FCS. CICLO INSTRUCCION ----------------
void ciclo_instruccion(int conexion_kernel);
void decode_instruccion(char* leido, int conexion_kernel);
void desalojo_proceso(t_sbuffer* buffer_contexto_proceso, int conexion_kernel, op_code mensaje_desalojo);
