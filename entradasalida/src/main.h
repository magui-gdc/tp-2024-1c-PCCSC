#include "filesystem.h"

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


int BLOCK_SIZE;     // estos no se si dejarlos pq ya estan en el struct del config
int BLOCK_COUNT;    // estos no se si dejarlos pq ya estan en el struct del config
int TAMANIO_ARCHIVO_BLOQUES; // = BLOCK_SIZE * BLOCK_COUNT

void cargar_config_struct_IO_gen(t_config*,t_config*);
void cargar_config_struct_IO_in(t_config*,t_config*);
void cargar_config_struct_IO_out(t_config*,t_config*);
void cargar_config_struct_IO_fs(t_config*,t_config*);

void paquete(int);

t_io* inicializar_io(char*, t_config*,t_config*);

IO_class selector_carga_config(t_config*,t_config*);

void responder_kernel(int socket_k);

/*      INSTRUCCIONES IO     */ 
void io_gen_sleep(uint32_t work_u);
void io_stdin_read(t_sbuffer* direcciones_memoria, uint32_t size, int socket);
void io_stdout_write(t_sbuffer* direcciones_memoria, uint32_t size, int socket);
