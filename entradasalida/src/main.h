#include <utils/utilsCliente.h>
#include <commons/config.h>
#include<readline/readline.h>
#include <utils/config.h>

typedef struct
{
    char* tipo_interfaz;
    char* tiempo_unidad_trabajo;
    char* ip_kernel;
    char* puerto_kernel;
    char* ip_memoria;
    char* puerto_memoria;
    char* path_base_dialfs;
    char* block_size;
    char* block_count
} config_struct;

extern config_struct config;

void cargar_config_struct_IO(t_config*);

void paquete(int);