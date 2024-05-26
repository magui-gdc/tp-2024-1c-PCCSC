#include <utils/utilsCliente.h>
#include <commons/config.h>
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
    char* block_count;
    char* retraso_compactacion;
} config_struct;

typedef enum {
    IO_GEN_SLEEP,
    IO_STDIN_READ,
    IO_STDOUT_WRITE,
    IO_FS_CREATE,
    IO_FS_DELETE,
    IO_FS_TRUNCATE
} instrucciones;

//nos llega el char de la interfaz (mouse, teclado, impresora), y el config que SUPONEMOS que viene del kernel segun la interfaz
typedef struct
{
    char* nombre_id;
} t_io;


extern config_struct config;

void cargar_config_struct_IO_gen(t_config*);
void cargar_config_struct_IO_in(t_config*);
void cargar_config_struct_IO_out(t_config*);
void cargar_config_struct_IO_fs(t_config*);

void paquete(int);

void inicializar_io(char*, t_config*);

void selector_carga_config(t_config*);