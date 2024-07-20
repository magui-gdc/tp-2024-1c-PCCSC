#include <utils/utilsCliente.h>
#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/config.h>
#include <commons/bitarray.h>
#include <utils/buffer.h>
#include <semaphore.h>

typedef struct { 
    int BLOQUE_INICIAL;
    int TAMANIO_ARCHIVO;
} archivo_metadata;

typedef struct{
    char* tipo_interfaz;
    int tiempo_unidad_trabajo;
    char* ip_kernel;
    char* puerto_kernel;
    char* ip_memoria;
    char* puerto_memoria;
    char* path_base_dialfs;
    int block_size;
    int block_count;
    int retraso_compactacion;
} config_struct;

typedef struct {
    char* nombre_archivo_dialfs;
    int bloque_inicial; 
} t_archivos_metadata;

extern t_bitarray *bitmap_bloques;
extern FILE* bloques_dat;
extern FILE* bitmap_dat;
// extern char* path_bloques; NO se usa (?)
extern config_struct config;
extern t_list* lista_archivos_abiertos;

/*          NACIMIENTO            */

void create_bloques_dat();
void init_bitmap_bloques(); //usar bitarray de las commons
void init_metadata(); //usar commons como en los config, es un .txt
void cargar_archivos_existentes();


/*          MUERTE            */
void close_bloques_dat();
void destroy_bitmap_bloques();


/*          FUNCIONES PRINCIPALES            */
void fs_create();
void compactar_bloques(uint32_t pid);
void crear_archivo(const char* nombre_archivo, t_bitarray* bitarray);   //t_bitarray ES DE LAS COMMONS!
void eliminar_archivo(const char* nombre_archivo, t_bitarray* bitarray);
void truncar_archivo(const char* nombre_archivo, int nuevo_tamano, t_bitarray* bitarray);
void leer_archivo(const char* nombre_archivo, int offset, int size, char* buffer, t_bitarray* bitarray);
void escribir_archivo(const char* nombre_archivo, int offset, int size, const char* buffer, t_bitarray* bitarray);
void compactar_fs(t_bitarray* bitarray);
bool leer_metadata(const char* nombre_archivo, archivo_metadata* metadata);
bool escribir_metadata(const char* nombre_archivo, const archivo_metadata* metadata);
char* obtener_path();
void escribir_bloquesdat();
void cargar_bloque(uint32_t pointer, char* contenido);
int contar_bloques_libres();
int buscar_bloques_libres_contiguos(int cant_bloques, int pos_inicial_buscada);

void asignar_bloque(int bloque);
void desasignar_bloque(int bloque);
t_archivos_metadata* buscar_por_bloque_inicial(int bloque_inicial);

/*      FUNCIONES IO FS     */ 
int io_fs_create(uint32_t pid, char* arch);
void io_fs_delete(uint32_t pid, char* arch);
void io_fs_truncate(uint32_t pid, char* arch, uint32_t reg_s);
void io_fs_write(uint32_t pid, char* arch, uint32_t bytes, uint32_t reg_p, int socket_m);
char* io_fs_read(uint32_t pid, char* arch, uint32_t bytes, uint32_t reg_p, int socket_m); 


/*      COMPACTACION     */ 

int primer_bloque_libre();
int primer_bloque_usado(int bloque_libre);
void compactar();
//void preparar_compactacion(uint32_t proceso, int bloque_inicial, int cant_bloques, int nueva_cant_bloques, int pos_inicial_nueva, int nuevo_tamanio, t_config* archivo_metadata_config);

/*          AUXILIARES            */
void actualizar_bitmap_dat(); // ACTUALIZAR ARCHIVO DE BITMAP SEGUN BITMAP EN MEMORIA
bool check_size(uint32_t size, uint32_t pointer, FILE* archivo); //el puntero al metadata, puntero al nt bloque inicial, int tam total 
