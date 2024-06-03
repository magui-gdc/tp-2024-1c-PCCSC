#include <utils/utilsCliente.h>
#include <commons/config.h>
#include <utils/config.h>
#include <semaphore.h>

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
typedef struct
{
    char* nombre_id;
    t_config* archivo;
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

void inicializar_io(char*, t_config*);

void selector_carga_config(t_config*);

////// DIALFS

// FUNCIONES PRINCIPALES
void crear_archivo(const char* nombre_archivo, t_bitarray* bitarray);   //t_bitarray ES DE LAS COMMONS!
void eliminar_archivo(const char* nombre_archivo, t_bitarray* bitarray);
void truncar_archivo(const char* nombre_archivo, int nuevo_tamano, t_bitarray* bitarray);
void leer_archivo(const char* nombre_archivo, int offset, int size, char* buffer, t_bitarray* bitarray);
void escribir_archivo(const char* nombre_archivo, int offset, int size, const char* buffer, t_bitarray* bitarray);
void compactar_fs(t_bitarray* bitarray);
bool leer_metadata(const char* nombre_archivo, archivo_metadata* metadata);
bool escribir_metadata(const char* nombre_archivo, const archivo_metadata* metadata);