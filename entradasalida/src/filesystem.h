#include <utils/utilsCliente.h>
#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/config.h>
#include <commons/bitarray.h>
#include <utils/buffer.h>
#include <semaphore.h>

extern t_bitarray *bitmap_bloques;
extern FILE* bloques_dat;
extern FILE* bitmap_dat;
extern char* path_bloques = strcat(config.path_base_dialfs, "/bloques.dat");

/*          NACIMIENTO            */

void create_bloques_dat();
void init_bitmap_bloques(); //usar bitarray de las commons
void init_metadata(); //usar commons como en los config, es un .txt


/*          MUERTE            */
void close_bloques_dat();
void destroy_bitmap_bloques();


/*          FUNCIONES PRINCIPALES            */
void f_create();
void compactar_bloques();
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
void cargar_bloque(FILE* archivo_metadata, char* contenido);


/*      COMPACTACION     */ 

int primer_bloque_libre();
int primer_bloque_usado();
void compactar();

/*          AUXILIARES            */
void actualizar_bitmap_dat(); // ACTUALIZAR ARCHIVO DE BITMAP SEGUN BITMAP EN MEMORIA
bool check_size(uint32_t size, uint32_t pointer); //el puntero al metadata, puntero al nt bloque inicial, int tam total 
