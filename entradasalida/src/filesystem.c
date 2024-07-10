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
    bloques_dat = fopen(path, "wb"); //un issue recomendo abrirlo con wb

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

    actualizar_bitmap_dat();
}

void actualizar_bitmap_dat(){
    char * path = strcat(config.path_base_dialfs , "/bitmap.dat");
    bitmap_dat = fopen(path, "wb");//un issue recomendo abrirlo con wb
    
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
    if (contar_bloques_libres() >= 1){
        char * path = strcat(config.path_base_dialfs, nombre_archivo);
        
        FILE* archivo_metadata = fopen(path, "w");
        int bloque_inicial = buscar_bloques_libres_contiguos(1);
        
        fwrite(bloque_inicial, sizeof(int), 1, archivo_metadata); // bloque inicial
        fwrite(0, sizeof(int), 1, archivo_metadata); // tamaño de archivo
        fclose(archivo_metadata);

        bitarray_set_bit(bitmap_bloques, bloque_inicial);
        actualizar_bitmap_dat();

    
    
    //cargar_bloque(archivo_metadata);
    
        log_crear_archivo(/*pid,*/ nombre_archivo);
    } else {
        log_error(logger, "No hay bloques disponibles en el FS");
    }
}

void io_fs_delete(/*uint32_t pid,*/ char* nombre_archivo){
    int bloque_inicial;
    int tamanio;
    int bloque_final;
    char * path = strcat(config.path_base_dialfs, nombre_archivo);
    
    FILE* archivo_metadata = fopen(path, "r");
    fread(&bloque_inicial, sizeof(int), 1, archivo_metadata);
    fread(&tamanio, sizeof(int), 1, archivo_metadata);
    
        double division = (double)tamanio / config.block_size;
        bloque_final = bloque_inicial + ceil(division);
        for(int i = bloque_inicial; i < bloque_final; i++){
            bitarray_clean_bit(bitmap_bloques, i);
        }
    
    fclose(archivo_metadata);
    remove(path);
}

void io_fs_truncate(char* nombre_archivo, uint32_t nuevo_tamanio){
    int bloque_inicial;
    int bloque_final;
    int tamanio;

    int cant_bloques = (int) ceil( (double)tamanio / config.block_size);
    int nueva_cant_bloques = (int) ceil( (double)nuevo_tamanio / config.block_size);


    char * path = strcat(config.path_base_dialfs, nombre_archivo);  
    FILE* archivo_metadata = fopen(path, "r");
    fread(&bloque_inicial, sizeof(int), 1, archivo_metadata);
    fread(&tamanio, sizeof(int), 1, archivo_metadata);
    fclose(archivo_metadata);

    FILE* archivo_metadata = fopen(path, "w");

    if(cant_bloques < nueva_cant_bloques){
        int cant_a_agregar = cant_bloques - nueva_cant_bloques;

        if (contar_bloques_libres() >= cant_a_agregar){
            int pos_inicial_contigua = buscar_bloques_libres_contiguos(cant_a_agregar, bloque_inicial);
            int pos_inicial_nueva;
            if(pos_inicial_contigua =! -1){
                //agregar a continuacion del bloque final

            } else if (pos_inicial_nueva =! -1){
                pos_inicial_nueva = buscar_bloques_libres_contiguos(nueva_cant_bloques, -1);

                //guardar bloques escritos
                //liberar bit de archivo del bitmap
                // asignar bloques libre
                compactar_bloques();

            } else { // rejuntar todos bloques sueltos y escribir ahi
                //guardar bloques escritos
                //liberar bit de archivo del bitmap
                compactar_bloques();
                pos_inicial_nueva = buscar_bloques_libres_contiguos(nueva_cant_bloques, -1);
            }
        } else {
            log_error(logger, "No hay mas bloques libres para realizar el truncate")
        }

        for(int i = bloque_inicial; i < bloque_inicial + nueva_cant_bloques; i++){
            
        }
    } else  if(cant_bloques > nueva_cant_bloques){
        //for(int i = nueva_cant_bloques; i  nueva_cant_bloques; i++){
        //    
        //}
    }

    fwrite(bloque_inicial, sizeof(int), 1, archivo_metadata); // bloque inicial
    fwrite(nuevo_tamanio, sizeof(int), 1, archivo_metadata); // tamaño de archivo
    fclose(archivo_metadata);
    
    fread(&tamanio, sizeof(int), 1, archivo_metadata);
    
        double division = (double)tamanio / config.block_size;
        bloque_final = bloque_inicial + ceil(division);
        
    
    
}

void io_fs_write(char* nombre_archivo, uint32_t direc, uint32_t size, uint32_t pointer, int socket){
    
    char* path = strcat(config.path_base_dialfs, nombre_archivo);
    FILE* archivo_metadata = fopen(path, "r+"); //abrimos el archivo metadata y nos posicionamos al principio


    check_size(size, pointer, archivo_metadata);//en base al offset me fijo si excede el tamano del archivo
    //size cuanto quiero escrbibir, pointer es desde donde quiero escrbibir,
    // del metadata saco la posicion inicial y el tam total
    //check que lo que queda en el metadata entre 
     
    uint32_t asig_esp = sizeof(char) * size;
    char* output = malloc(asig_esp);

    t_sbuffer* buffer_memoria = buffer_create(
        sizeof(int) // respuesta de memoria
        + sizeof(uint32_t) // direccion de memoria
        + (uint32_t)strlen(output) + sizeof(uint32_t) // string que dara memoria
    );

    
    buffer_add_uint32(buffer_memoria, direc);
    //TODO: aca no va IO_FS_WRITE, sino la instruccion que corresponda al cargado de datos en memoria (que puede ser esta misma si quieren)
    cargar_paquete(socket, IO_FS_WRITE, buffer_memoria);
    int ok = 0;
    while (ok == 0){ // este while tal vez esta de mas
        int op_code = recibir_operacion(socket);
        if(op_code == IO_MEMORY_DONE){ //TODO: esto no es un semaforo? podriamos hacer un semaforo que se comunique entre memoria-IO?
            ok = buffer_read_int(buffer_memoria); //?
            direc = buffer_read_uint32(buffer_memoria); //lo avanzo uno pero en realidad no me importa obtener este valor
            output = buffer_read_string(buffer_memoria, &size); 

            if(ok == -1){ //?
                log_error(logger, "Error al descargar el valor de memoria");
                exit(EXIT_FAILURE);
            }
        }
    }
    
    
    cargar_bloque(archivo_metadata, pointer, output);
    
    buffer_destroy(buffer_memoria);
}

void io_fs_read(char* nombre_archivo, uint32_t direc, uint32_t size, uint32_t pointer, int socket){

}

/*      MANEJO BLOQUES Y BITMAP     */

int buscar_bloques_libres_contiguos(int cant_bloques, int pos_inicial_buscada){
    int bloques_encontrados = 0;
    if(pos_inicial_buscada == -1){ // Si es -1, entonces no nos importa la posicion de partida.
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
        return -1; //si llego hasta aca, entonces no se encontraron n bloques contiguos en todo el bitmap
    } else {
        for(int i=pos_inicial_buscada, i<config.block_count, i++){
            if(!bitarray_test_bit(bitmap_bloques, i)){
                bloques_encontrados++;
            }
            if(bloques_encontrados == cant_bloques) {
                return pos_inicial_buscada 
            } else { return -1 } ; //devuelve -1 si no hay suficientes bloques contiguos para el truncate
        }
        return (contar_bloques_libres() >= cant_bloques) ? compactar_bloques() : -1;
    }
}

int contar_bloques_libres(){
    int bloques_encontrados = 0;
    for(int i=0, i<config.block_count, i++){
        if(!bitarray_test_bit(bitmap_bloques, i)){
            bloques_encontrados ++;
        }
    }
    return bloques_encontrados;
}


void cargar_bloque(FILE* archivo_metadata, uint32_t offset, char* contenido){  //reg_puntero, contenido de memoria
    // el reg puntero te dice la direccion del metadata (introducido por param)
    
    // en base a la direccion del metadata, se obtiene el bloque inicial
    uint32_t bloque_inicial;
    fread(bloque_inicial, sizeof(int), 1, archivo_metadata);

    // posiciono el FILE donde quiero escribir dentro del archivo
    fseek(bloques_dat, bloque_inicial*config.block_size + offset);
    
    uint32_t cantidad_bloques = ceil(sizeof(contenido) / config.block_size);
    
    // escribo
    fwrite(contenido, config.block_size, cantidad_bloques, bloques_dat) //esto puede excederse del archivo, pero está confiando de que no puede llegar un archivo que exceda por el checkeo
    //chequear que bloque se haya cargado bien
    if(/*bloque mal cargado*/){
        log_error(logger,"Error al cargar bloque.")
        return EXIT_FAILURE;
    }
}

void escribir_bloquesdat(FILE* archivo_metadata, int reg_d){

    
    
    //reg_d = 
    
    

    
}

/*      COMPACTACION     */ 

int primer_bloque_libre(){
    for(int i=0, i<config.block_count, i++){
        (bitarray_test_bit(bitmap_bloques, i)) : return i;
    }
    return -1;
}

int primer_bloque_usado(){
    for(int i=0, i<config.block_count, i++){
        (!bitarray_test_bit(bitmap_bloques, i)) : return i;
    }
    return -1;
}

void compactar_bloques(){
    int primer_bloque_libre = primer_bloque_libre();
    int primer_bloque_usado = primer_bloque_usado();
    char* bloque_aux = (char*)malloc(config.block_size);

    if(primer_bloque_libre == -1 || primer_bloque_usado == -1){
        log_error(logger, "No se que queres compactar, todos lo bloques estan ocupados.");
        return EXIT_FAILURE;
    }

    while(primer_bloque_libre<config.block_count && primer_bloque_usado<config.block_count){
        if(primer_bloque_libre < primer_bloque_usado){ 
            fseek(bloques_dat, primer_bloque_usado*config.block_size, SEEK_SET);
            fread(bloque_aux, 1, config.block_size, bloques_dat);

            fseek(bloques_dat, primer_bloque_libre*config.block_size, SEEK_SET);
            fwrite(bloque_aux, 1, config.block_size, bloques_dat);

            bitarray_set_bit(bitmap_bloques, primer_bloque_libre);
            bitarray_clean_bit(bitmap_bloques, primer_bloque_usado);
            
            free(bloque_aux);

            primer_bloque_libre = primer_bloque_libre();
            primer_bloque_usado = primer_bloque_usado();
        }
        else{
            primer_bloque_libre++;
            primer_bloque_usado++;
        }
    }
    actualizar_bitmap_dat();
}

bool check_size(uint32_t size, uint32_t pointer, FILE* archivo_metadata){

    int32_t bloque_inicial;
    uint32_t tamanio_archivo;
    fread(bloque_inicial, sizeof(int), 1, archivo_metadata);
    fread(tamanio_archivo, sizeof(int), 1, archivo_metadata);


    
}
