#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <string.h>
#include "filesystem.h"
#include "main.c"


/*          NACIMIENTO            */

void create_bloques_dat(){
    char * path = strcat(config.path_base_dialfs, "/bloques.dat");

    bloques_dat = fopen(path, "w");
    if(bloques_dat == NULL){
        log_error(logger,"Error al inicializar bloques.dat");
        exit(EXIT_FAILURE);
    }

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

    actualizar_bitmap_dat();
}

void actualizar_bitmap_dat(){
    char * path = strcat(config.path_base_dialfs, "/bitmap.dat");
    bitmap_dat = fopen(path, "w");
    
    for (size_t i = 0; i < config.block_count; i++) {
        bool bit = bitarray_test_bit(bitmap_marcos, i);
        char bit_char = bit + '0';
        fwrite(bit_char ,sizeof(char),1,bitmap_dat);
    }

    fclose(bitmap_dat);
}

void fs_create(){
    int tamanio_bloque = config.BLOCK_SIZE;
    
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