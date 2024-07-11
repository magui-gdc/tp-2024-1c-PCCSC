#include <utils/utilsCliente.h>
#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/config.h>
#include <commons/bitarray.h>
#include <utils/buffer.h>
#include <semaphore.h>
#include "filesystem.h"

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

/*
typedef enum {
    IO_GEN_SLEEP,
    IO_STDIN_READ,
    IO_STDOUT_WRITE,
    IO_FS_CREATE,
    IO_FS_DELETE,
    IO_FS_TRUNCATE
} instrucciones;
*/

typedef enum {
    GEN,
    IN,
    OUT,
    FS,
    MISSING
} IO_class;

//nos llega el char de la interfaz (mouse, teclado, impresora), y el config que SUPONEMOS que viene del kernel segun la interfaz
typedef struct{
    char* nombre_id;
    t_config* archivo;
    IO_class clase;
} t_io;


typedef struct { 
    int BLOQUE_INICIAL;
    int TAMANIO_ARCHIVO;
} archivo_metadata;

int BLOCK_SIZE;     // estos no se si dejarlos pq ya estan en el struct del config
int BLOCK_COUNT;    // estos no se si dejarlos pq ya estan en el struct del config
int TAMANIO_ARCHIVO_BLOQUES; // = BLOCK_SIZE * BLOCK_COUNT


extern config_struct config;

void cargar_config_struct_IO_gen(t_config*);
void cargar_config_struct_IO_in(t_config*);
void cargar_config_struct_IO_out(t_config*);
void cargar_config_struct_IO_fs(t_config*);

void paquete(int);

t_io* inicializar_io(char*, t_config*);

IO_class selector_carga_config(t_config*);

void responder_kernel(int socket_k);

// INSTRUCCIONES DE IO
void io_gen_sleep(uint32_t work_u);
void io_stdin_read(t_sbuffer* direcciones_memoria, uint32_t size, int socket);
void io_stdout_write(t_sbuffer* direcciones_memoria, uint32_t size, int socket);
void io_fs_create(uint32_t proceso, char* arch, int socket_m);
void io_fs_delete(uint32_t proceso, char* arch, int socket_m);
void io_fs_truncate(char* arch, uint32_t reg_s, int socket_m);
void io_fs_write(char* arch, uint32_t reg_d, uint32_t reg_s, uint32_t reg_p, int socket_m);
void io_fs_read(char* arch, uint32_t reg_d, uint32_t reg_s, uint32_t reg_p, int socket_m); //TODO: estos ultimos dos, como dije en cpu, no se si el reg_p es un entero
