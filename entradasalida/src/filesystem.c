#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "filesystem.h"
#include "main.c"

void init_bitmap_bloques(){
    size_t tamanio_bitmap = config.block_size*config.block_count; 
    
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
}