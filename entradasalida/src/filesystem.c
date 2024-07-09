#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <string.h>
#include "filesy00stem.h"
#include "main.c"
#include "logs.c"

extern t_config* config;

/*          NACIMIENTO            */

void create_bloques_dat(char* path_base){
    char * path = strcat(config.path_base_dialfs, "/bloque.dat");
    bloques_dat = fopen(path, "w");

    if(bloques_dat == NULL){
        log_error(logger,"Error al inicializar bloques.dat");
        exit(EXIT_FAILURE);
    }

    // y como sigue esto?
}

void init_bitmap_bloques(){
    size_t tamanio_bitmap = config.block_count; 
    
    char *bitmap = malloc(tamanio_bitmap); // tamanio en bytes que se va a reservar
    if (!bitmap) {
        log_error(logger,"Error en la creación del Bitmap de Bloques Libres");
        exit(EXIT_FAILURE);
    }

    bitmap_bloques = bitarray_create_with_mode(bitmap, tamanio_bitmap, LSB_FIRST);

    // inicializa todos los bits a 0 (todos los bloques están libres)
    for (size_t i = 0; i < config.block_count; i++) {
        bitarray_clean_bit(bitmap_bloques, i);
    }

    actualizar_bitmap_dat(path_base);
}

void actualizar_bitmap_dat(){
    char * path = strcat(config.path_base_dialfs , "/bitmap.dat");
    bitmap_dat = fopen(path, "w");
    
    for (size_t i = 0; i < config.block_count; i++) {
        bool bit = bitarray_test_bit(bitmap_bloques, i);
        char bit_char = bit + '0';
        fwrite(bit_char ,sizeof(char),1,bitmap_dat);
    }

    fclose(bitmap_dat);
}

void fs_create(){
    
    create_bloques_dat();
    init_bitmap_bloques();
    
}

/*          MUERTE            */

void close_bloques_dat(){
    fclose(bloques_dat);
    log_debug(logger, "Archivos bloques.dat cerrado.");
}

void destroy_bitmap_bloques(){
    bitarray_destroy(bitmap_bloques);
}

/*      PETICIONES      */

void io_fs_create(/*uint32_t pid,*/ char* nombre_archivo){
    char * path = strcat(config.path_base_dialfs, nombre_archivo);
    
    //buscar bit libre
    //ARCHIVOS
    /*
    bloques_dat (extern ya inicializado)
    bitmap_dat (extern ya inicializado)
    archivo_metadata (por inicializar ahora)
    */
    FILE* archivo_metadata = fopen(path, "a+");
    int bloque_inicial = asignar_bloque();

    fwrite(bloque_inicial, sizeof(int), 1, archivo_metadata); // bloque inicial
    fwrite(0, sizeof(int), 1, archivo_metadata); // tamaño de archivo
    //cargar_bloque(archivo_metadata);
    
    log_crear_archivo(pid, nombre_archivo);
}

void io_fs_delete(char* nombre_archivo, int socket){

}

void io_fs_truncate(char* nombre_archivo, uint32_t size, int socket){
    
}

void io_fs_write(char* nombre_archivo, uint32_t direc, uint32_t size, uint32_t pointer, int socket){

}

void io_fs_read(char* nombre_archivo, uint32_t direc, uint32_t size, uint32_t pointer, int socket){

}

/*      MANEJO BLOQUES Y BITMAP     */

int buscar_bloques_libres(int cant_bloques){
    int bloques_encontrados = 0;
    for(int i=0, i<config.block_count, i++){
        if(!bitarray_test_bit(bitmap_bloques, i)){
            bloques_encontrados++;
            int pos_referencia = i;
            for (pos_referencia; pos_referencia < pos_referencia + cant_bloques; pos_referencia++){
                if(bitarray_test_bit(bitmap_bloques, i)){ //si aun no llegamos a la cant buscada, y dio ocupado, este de referencia no nos sirve.
                    bloques_encontrados = 0;
                    break;
                } else {
                    bloques_encontrados++;
                }
            }
            if(bloques_encontrados == cant_bloques) return i; //devolvemos el inicial de la lista de n bloques vacios contiguos.
        }
    }
    //si llego hasta aca, entonces no se encontraron n bloques contiguos.
    //contar todos los bloques disponibles. -> Si alcanzan, se compacta, sino, no hay espacio en FS.
}



void cargar_bloque(FILE* archivo_metadata, int cant_bloques){
    int posicion = posicion_bloque_libre();
    if(posicion==0){
        log_error(logger,"No hay bloques libres.")
        return EXIT_FAILURE;
    }

    // el reg_direccion te dan la direccion del metadata
    
    // en base a la direccion del metadata, se obtiene el bloque inicial
    
    // con reg_tamaño se define el límite donde escribir

    // se escribe en el bloque el reg_puntero

    fseek(bloques_dat, posicion*config.block_size);
    
    //fwrite(archivo_metadata, sizeof(archivo_metadata), 1, bloques_dat);
    
    fwrite(contenido /*reg_puntero*/, config.block_size, cantidad_bloques, bloques_dat)
    //chequear que bloque se haya cargado bien
    if(/*bloque mal cargado*/){
        log_error(logger,"Error al cargar bloque.")
        return EXIT_FAILURE;
    }

    bitarray_set_bit(bitmap_bloques, posicion);
}

void cargar_bitmap(int posicion){
    
}

void escribir_bloquesdat(FILE* archivo_metadata, int reg_d){

    
    
    //reg_d = 
    
    

    
}

