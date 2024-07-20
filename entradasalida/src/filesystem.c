#include <utils/hello.h>
#include <string.h>
#include "filesystem.h"
#include <sys/mman.h>  
#include <math.h>
#include "logs.h"
#include <dirent.h>

config_struct config;
// char *path_bloques; NO se usa(?)
t_bitarray *bitmap_bloques;
FILE *bloques_dat;
FILE *bitmap_dat;
t_list* lista_archivos_abierto;

//     NACIMIENTO

// crea bitmap y bitmap.dat
void create_bloques_dat(){
    char path[256];
    strcpy(path, config.path_base_dialfs);
    strcat(path, "/bloque.dat");
    log_debug(logger, "ruta bloquess %s", path);
    bloques_dat = fopen(path, "wb+"); //un issue recomendo abrirlo con wb

    if (bloques_dat == NULL){
        log_error(logger, "Error al inicializar bloques.dat");
        exit(EXIT_FAILURE);
    }
}

// este incializa bitarray de bits de bloque y crea bitmap.dat y lo asocia a el bitarray con un mmap
void init_bitmap_bloques(){
    char path[256];
    strcpy(path, config.path_base_dialfs);
    strcat(path, "/bitmap.dat");
    bitmap_dat = fopen(path, "wb+"); // un issue recomendo abrirlo con wb+

    if (bitmap_dat == NULL){
        log_error(logger, "Error al inicializar bloques.dat");
        exit(EXIT_FAILURE);
    }

    size_t tamanio_bitmap = config.block_count / 8; // SE TIENE QUE DIVIDIR POR 8 PORQUE EL BITARRAY TRABAJA CON BITS!!
    if (config.block_count % 8 != 0) {
        tamanio_bitmap++;  // se anañe un byte extra si hay bits adicionales parciales que no llegarían a completar el byte
    }

    if (ftruncate(fileno(bitmap_dat), tamanio_bitmap + 1) == -1) {
        log_error(logger, "Error al establecer el tamaño del archivo bitmap.dat");
        fclose(bitmap_dat);
        exit(EXIT_FAILURE);
    }

    //  MMAP: función que mapea un archivo o dispositivo en memoria. Esto significa que el contenido del archivo puede ser accedido directamente desde la memoria, sin la necesidad de leer/escribir el archivo cada vez. 
    // Se le pasa por parámetro la dirección donde se empezará a mapear la memoria (NULL permite al kernel elegir la dirección), la longitud del area a mapear, la proteccion de memoria, el comportamiento del mapeo, el descriptor de archivo del archivo a mapear y el offset en el archivo para empezar el mapeo. 
    // Retorna  un puntero (void*) a la memoria mapeada si la operación es exitosa.
    void* bitmapPointer = mmap(NULL, tamanio_bitmap, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(bitmap_dat), 0);
    if (bitmapPointer == MAP_FAILED) {
        log_error(logger, "Error al mapear el archivo bitmap.dat en memoria");
        fclose(bitmap_dat);
        exit(EXIT_FAILURE);
    }

    memset(bitmapPointer, 0, tamanio_bitmap);

    // creo bitarray 
    bitmap_bloques = bitarray_create_with_mode(bitmapPointer, tamanio_bitmap, LSB_FIRST);
    if (!bitmap_bloques) {
        log_error(logger, "Error al asignar memoria para el objeto bitarray");
        munmap(bitmapPointer, tamanio_bitmap);
        fclose(bitmap_dat);
        exit(EXIT_FAILURE);
    }

    // MSYNC: función que asegura que las modificaciones realizadas en el mapeo de memoria se escriban de vuelta al archivo. Se le pasa como parámetro la dirección de la memoria mapeada, la longitud del área a sincronizar y flags..
    if (msync(bitmapPointer, bitmap_bloques->size, MS_SYNC) == -1) {
        log_error(logger, "Error en la sincronización con msync()");
    }

    fclose(bitmap_dat);
}

void fs_create(){
    create_bloques_dat(); // este ya inicializa bitmap porque las estructuras están mapeadas == son lo mismo bitarray que bitmap.dat!
    init_bitmap_bloques();
    lista_archivos_abierto = list_create();
    cargar_archivos_existentes();
}

void cargar_archivos_existentes(){
    struct dirent *p_directorio;  // Declaración del puntero para la entrada del directorio
    char path[256];

    // opendir() devuelve un puntero de tipo DIR.
    DIR *directorio = opendir(config.path_base_dialfs);

    if (directorio == NULL) {  // opendir devuelve NULL si no puede abrir el directorio
        printf("No se pudo abrir el directorio");
        exit(EXIT_FAILURE);
    }

    // Readdir devuelve un puntero a struct dirent
    while ((p_directorio = readdir(directorio)) != NULL) {

        log_debug(logger, "lei el archivo %s", p_directorio->d_name);
        if (strcmp(p_directorio->d_name, ".") != 0 && strcmp(p_directorio->d_name, "..") != 0 && strcmp(p_directorio->d_name, "bloque.dat") != 0 && strcmp(p_directorio->d_name, "bitmap.dat") != 0){
            // guardar estrucutra del archivo en LISTA
            
            strcpy(path, config.path_base_dialfs);
            strcat(path, "/");
            strcat(path, p_directorio->d_name);

            t_config* archivo_metadata_config = config_create((char*)path);
            log_debug(logger, "el path es %s", path);
            int bloque_inicial = config_get_int_value(archivo_metadata_config, "BLOQUE_INICIAL");
            t_archivos_metadata* archivo_nuevo = malloc(sizeof(t_archivos_metadata));
            archivo_nuevo->nombre_archivo_dialfs = malloc(strlen(p_directorio->d_name) + 1); // espacio para el '/0'
            strcpy(archivo_nuevo->nombre_archivo_dialfs, p_directorio->d_name);
            archivo_nuevo->bloque_inicial = bloque_inicial;
            
            list_add(lista_archivos_abierto, archivo_nuevo);

            config_destroy(archivo_metadata_config);
        }
    }
    closedir(directorio);  
}

// MUERTE

void close_bloques_dat(){
    fclose(bloques_dat);
    log_debug(logger, "Archivos bloques.dat cerrado.");
}

void destroy_bitmap_bloques(){
    bitarray_destroy(bitmap_bloques);
}

// PETICIONES

int io_fs_create(uint32_t pid, char *nombre_archivo){
    log_info(logger, "PID: %u - Operacion: IO_FS_CREATE", pid);
    if (contar_bloques_libres() >= 1){
        char path[64];
        strcpy(path, config.path_base_dialfs);
        sprintf(path + strlen(path), "/%s", nombre_archivo);

        FILE *archivo_metadata = fopen(path, "r"); // en modo lectura para corroborar si el archivo ya fue creado por otro proceso
        if (archivo_metadata != NULL){
            log_debug(logger, "el archivo %s ya existia", nombre_archivo);
            fclose(archivo_metadata);
        } else {
            archivo_metadata = fopen(path, "w"); // El archivo no existía => se crea!
            if (archivo_metadata == NULL){
                log_error(logger, "Error al abrir/crear archivo metadata %s del proceso %u.",nombre_archivo, pid);
                exit(EXIT_FAILURE);
            }
            log_debug(logger, "el archivo %s no existia y se creo", nombre_archivo);
            fclose(archivo_metadata);

            int bloque_inicial = buscar_bloques_libres_contiguos(1, -1);
            char bloque_inicial_str[12];
            char tamanio_inicial_str[12];
            sprintf(bloque_inicial_str, "%d", bloque_inicial);
            sprintf(tamanio_inicial_str, "%d", 0);

            t_config* archivo_metadata_config = config_create((char*)path); // se trata al archivo_metadata como un archivo de configuracion!
            config_set_value(archivo_metadata_config, "BLOQUE_INICIAL", bloque_inicial_str);
            config_set_value(archivo_metadata_config, "TAMANIO_ARCHIVO", tamanio_inicial_str);
            config_save(archivo_metadata_config);

            t_archivos_metadata* archivo_nuevo = malloc(sizeof(t_archivos_metadata));
            archivo_nuevo->nombre_archivo_dialfs = malloc(strlen(nombre_archivo) + 1); // espacio para el '/0'
            strcpy(archivo_nuevo->nombre_archivo_dialfs, nombre_archivo);
            archivo_nuevo->bloque_inicial = bloque_inicial;

            list_add(lista_archivos_abierto, archivo_nuevo);

            config_destroy(archivo_metadata_config); // se libera el config después de guardar la configuración del nuevo archivo metadata
            asignar_bloque(bloque_inicial); // hace el set_bit y luego impacta en el bitmap.dat
            log_crear_archivo(logger, pid, nombre_archivo);
        }
        return 1;           
    }
    else{
        log_error(logger, "No hay bloques disponibles en el FS");
        return 0;
    }
}

void io_fs_delete(uint32_t pid, char *nombre_archivo){
    log_info(logger, "PID: %u - Operacion: IO_FS_DELETE", pid);
    int bloque_final;
    char path[64];
    strcpy(path, config.path_base_dialfs);
    sprintf(path + strlen(path), "/%s", nombre_archivo);

    t_config* archivo_metadata_config = config_create((char*)path);
    int bloque_inicial = config_get_int_value(archivo_metadata_config, "BLOQUE_INICIAL");
    int tamanio_archivo = config_get_int_value(archivo_metadata_config, "TAMANIO_ARCHIVO");

    double division = (double)tamanio_archivo / config.block_size;
    bloque_final = bloque_inicial + ceil(division);
    for (int i = bloque_inicial; i < bloque_final; i++){
        desasignar_bloque(i); // hace el clean_bit y actualiza en bitmap.dat
    }

    t_archivos_metadata* archivo_a_eliminar =  buscar_por_bloque_inicial(bloque_inicial);
    list_remove_element(lista_archivos_abierto, archivo_a_eliminar);
    free(archivo_a_eliminar->nombre_archivo_dialfs);
    free(archivo_a_eliminar);

    config_destroy(archivo_metadata_config);
    int resultado = remove(path);
    if (resultado != 0) {
        log_error(logger, "Error al eliminar el archivo %s del proceso %u.", nombre_archivo, pid);
        exit(EXIT_FAILURE);
    } else 
        log_eliminar_archivo(logger, pid, nombre_archivo);
}

void io_fs_truncate(uint32_t pid, char *nombre_archivo, uint32_t nuevo_tamanio){
    log_info(logger, "PID: %u - Operacion: IO_FS_TRUNCATE", pid);
    char path[64];
    strcpy(path, config.path_base_dialfs);
    sprintf(path + strlen(path), "/%s", nombre_archivo);
    int bloque_final; 

    t_config* archivo_metadata_config = config_create((char*)path);
    int bloque_inicial = config_get_int_value(archivo_metadata_config, "BLOQUE_INICIAL");
    int tamanio_archivo = config_get_int_value(archivo_metadata_config, "TAMANIO_ARCHIVO");

    int cant_bloques = (tamanio_archivo == 0) ? 1 : (int)ceil((double)tamanio_archivo / config.block_size);
    int nueva_cant_bloques = (nuevo_tamanio == 0) ? 1 : (int)ceil((double)nuevo_tamanio / config.block_size);

    bloque_final = bloque_inicial + cant_bloques - 1; // indice del ultimo bloque ocupado por el archivo
    log_debug(logger, "recibe bloque final %d, blque inicial %d cant bloques actuales %d y nueva cantida %d", bloque_final , bloque_inicial, cant_bloques, nueva_cant_bloques);
    if (cant_bloques < nueva_cant_bloques)
    { // agregar bloques
        int cant_a_agregar = nueva_cant_bloques - cant_bloques;

        if (contar_bloques_libres() >= cant_a_agregar)
        {
            int pos_inicial_contigua = buscar_bloques_libres_contiguos(cant_a_agregar, (bloque_final + 1));
            int pos_inicial_nueva;
            log_debug(logger, "pos inicial contigua: %d", pos_inicial_contigua);
            if (pos_inicial_contigua != -1 && pos_inicial_contigua != -2){ // agregar a continuacion del bloque final
                for (int i = bloque_final+1; i < bloque_inicial + nueva_cant_bloques; i++){
                    asignar_bloque(i);
                }
                char tamanio_nuevo_str[12];
                sprintf(tamanio_nuevo_str, "%u", nuevo_tamanio);
                config_set_value(archivo_metadata_config, "TAMANIO_ARCHIVO", tamanio_nuevo_str);
                config_save(archivo_metadata_config);
                config_destroy(archivo_metadata_config);
            } else { // no hay bloques contiguos proximos después del archivo
                pos_inicial_nueva = buscar_bloques_libres_contiguos(nueva_cant_bloques, -1);
                if (pos_inicial_nueva != -3){ // encontraste suficientes bloques contiguos en bitmap
                    char *contenido = malloc(cant_bloques*config.block_size);
                    
                    fseek(bloques_dat, bloque_inicial * config.block_size, SEEK_SET);
                    fread(contenido, cant_bloques*config.block_size, 1, bloques_dat);

                    for (int i = bloque_inicial; i < bloque_inicial + cant_bloques; i++){
                        desasignar_bloque(i);
                    }

                    for (int i = pos_inicial_nueva; i < pos_inicial_nueva + nueva_cant_bloques; i++){
                        asignar_bloque(i);
                    }

                    fseek(bloques_dat, pos_inicial_nueva * config.block_size, SEEK_SET);
                    fwrite(contenido, 1, cant_bloques*config.block_size, bloques_dat);
                    fflush(bloques_dat);

                    char bloque_inicial_nuevo_str[12];
                    char tamanio_nuevo_str[12];
                    sprintf(bloque_inicial_nuevo_str, "%d", pos_inicial_nueva);
                    sprintf(tamanio_nuevo_str, "%u", nuevo_tamanio);
                    config_set_value(archivo_metadata_config, "BLOQUE_INICIAL", bloque_inicial_nuevo_str);
                    config_set_value(archivo_metadata_config, "TAMANIO_ARCHIVO", tamanio_nuevo_str);
                    config_save(archivo_metadata_config);

                    t_archivos_metadata* archivo_modificado = buscar_por_bloque_inicial(bloque_inicial);
                    archivo_modificado->bloque_inicial = pos_inicial_nueva;
                } else { //no hay suficientes bloques contiguos en bitmap
                    // rejuntar todos bloques sueltos y escribir ahi
                    // guardar bloques escritos
                    // liberar bit de archivo del bitmap
                    char *contenido = malloc(cant_bloques*config.block_size);
                    
                    fseek(bloques_dat, bloque_inicial * config.block_size, SEEK_SET);
                    fread(contenido, cant_bloques*config.block_size, 1, bloques_dat);

                    for (int i = bloque_inicial; i < bloque_inicial + cant_bloques; i++){
                        desasignar_bloque(i);
                    }

                    compactar_bloques(pid);
                    pos_inicial_nueva = buscar_bloques_libres_contiguos(nueva_cant_bloques, -1);

                    for (int i = pos_inicial_nueva; i < pos_inicial_nueva + nueva_cant_bloques; i++){
                        asignar_bloque(i);
                    }

                    fseek(bloques_dat, pos_inicial_nueva * config.block_size, SEEK_SET);
                    fwrite(contenido, 1, cant_bloques*config.block_size, bloques_dat);
                    fflush(bloques_dat);

                    char bloque_inicial_nuevo_str[12];
                    char tamanio_nuevo_str[12];
                    sprintf(bloque_inicial_nuevo_str, "%d", pos_inicial_nueva);
                    sprintf(tamanio_nuevo_str, "%u", nuevo_tamanio);
                    config_set_value(archivo_metadata_config, "BLOQUE_INICIAL", bloque_inicial_nuevo_str);
                    config_set_value(archivo_metadata_config, "TAMANIO_ARCHIVO", tamanio_nuevo_str);
                    config_save(archivo_metadata_config);

                    t_archivos_metadata* archivo_modificado = buscar_por_bloque_inicial(bloque_inicial);
                    archivo_modificado->bloque_inicial = pos_inicial_nueva;

                    config_destroy(archivo_metadata_config);
                    free(contenido);
                }
            }
        }
        else{
            log_error(logger, "No hay mas bloques libres para realizar el truncate");
            return;
        }
    } else if (cant_bloques > nueva_cant_bloques){
        for (int i = bloque_final; i > (bloque_final - (cant_bloques-nueva_cant_bloques)); i--){
            desasignar_bloque(i);
        }
        char tamanio_nuevo_str[12];
        sprintf(tamanio_nuevo_str, "%u", nuevo_tamanio);
        config_set_value(archivo_metadata_config, "TAMANIO_ARCHIVO", tamanio_nuevo_str);
        config_save(archivo_metadata_config);
        config_destroy(archivo_metadata_config);
    }
    
    log_truncar_archivo(logger, pid, nombre_archivo, nuevo_tamanio);
}

void io_fs_write(uint32_t pid, char *nombre_archivo, uint32_t size, uint32_t pointer, int socket){
    log_info(logger, "PID: %u - Operacion: IO_FS_WRITE", pid);
    log_debug(logger, "archivo con datos nombre %s, pid: %u, size %u, offset %u", nombre_archivo, pid, size, pointer);
    
    int recibir_operacion_memoria = recibir_operacion(socket);
    if (recibir_operacion_memoria == PETICION_LECTURA){
        t_sbuffer *buffer_rta_peticion_lectura = cargar_buffer(socket);
        int cantidad_peticiones = buffer_read_int(buffer_rta_peticion_lectura);

        void *peticion_completa = malloc(size);
        uint32_t bytes_recibidos = 0;
        for (size_t i = 0; i < cantidad_peticiones; i++){
            uint32_t bytes_peticion;
            void *dato_peticion = buffer_read_void(buffer_rta_peticion_lectura, &bytes_peticion);
            memcpy(peticion_completa + bytes_recibidos, dato_peticion, bytes_peticion);
            bytes_recibidos += bytes_peticion;
            free(dato_peticion);
        }

        char *output = malloc(size + 1); // +1 para el carácter nulo al final
        memcpy(output, peticion_completa, size);
        output[size] = '\0'; // YA TENES CARGADO EL VALOR LEIDO DESDE MEMORIA

        //// ESCRITURA EN ARCHIVO!
        char path[64];
        strcpy(path, config.path_base_dialfs);
        sprintf(path + strlen(path), "/%s", nombre_archivo);

        t_config* archivo_metadata_config = config_create((char*)path);
        int tamanio_archivo = config_get_int_value(archivo_metadata_config, "TAMANIO_ARCHIVO"); 
        // CHECK SIZE:
        int tamanio_real = ((tamanio_archivo == 0) ? 1 : (int)ceil((double)tamanio_archivo / config.block_size)) * config.block_size;

        if (pointer > tamanio_archivo) {
            log_error(logger,"El offset que pasaste es mas granque que el archivo");
            exit(EXIT_FAILURE);
        }

        uint32_t bytes_que_podemos_escribir = tamanio_real - pointer; // una posicion de bytes 
        char *partial_output;
        if(bytes_que_podemos_escribir < size){
            partial_output = malloc(bytes_que_podemos_escribir + 1); // +1 para el carácter nulo al final
            memcpy(partial_output, output, bytes_que_podemos_escribir);
        } else {
            partial_output = output;
        }

        fseek(bloques_dat, pointer, SEEK_SET);
        fwrite(partial_output, 1, bytes_que_podemos_escribir < size ? bytes_que_podemos_escribir : size, bloques_dat);
        fflush(bloques_dat);
        
        free(partial_output);
        /////

        log_escribir_archivo(logger, pid, nombre_archivo, size, pointer);
        free(peticion_completa);
        buffer_destroy(buffer_rta_peticion_lectura);

    }

}

char* io_fs_read(uint32_t pid, char *nombre_archivo, uint32_t size, uint32_t offset, int socket){
    log_debug(logger, "archivo con datos nombre %s, pid: %u, size %u, offset %u", nombre_archivo, pid, size, offset);
    log_info(logger, "PID: %u - Operacion: IO_FS_READ", pid);

    log_leer_archivo(logger, pid, nombre_archivo, size, offset);

    char path[64];
    strcpy(path, config.path_base_dialfs);
    sprintf(path + strlen(path), "/%s", nombre_archivo);

    char *datos_leidos = (char *)malloc(size + 1);
    fseek(bloques_dat, offset, SEEK_SET);
    fread(datos_leidos, size, 1, bloques_dat);

    return datos_leidos;

}

// MANEJO BLOQUES Y BITMAP

int buscar_bloques_libres_contiguos(int cant_bloques, int pos_inicial_buscada){
    int bloques_encontrados = 0;
    if (pos_inicial_buscada == -1){ // Si es -1, entonces no nos importa la posicion de partida.
        for (int i = 0; i < config.block_count; i++) {
            if (!bitarray_test_bit(bitmap_bloques, i)) { //0
                bloques_encontrados++;
                if (bloques_encontrados == cant_bloques) {
                    log_debug(logger, "SE ENCONTRARON %d BLOQUES LIBRES A PARTIR DEL BLOQUE %d", cant_bloques, i );
                    return i - cant_bloques + 1; // retorna la posición inicial de los bloques libres contiguos
                }
            } else { //1
                bloques_encontrados = 0; // hace un reset si se encuentra bloque ocupado
            }
        }
        log_debug(logger, "NO SE ENCONTRARON %d BLOQUES LIBRES CONJUNTOS EN TODO EL BITMAP. Hay que compactar.", cant_bloques);
        return -3; // No se encontraron bloques contiguos suficientes
    }
    else {
        for (int i = pos_inicial_buscada; i < config.block_count; i++)
        {
            if (!bitarray_test_bit(bitmap_bloques, i)) { //0
                bloques_encontrados++;
                
                if (bloques_encontrados == cant_bloques) {
                    log_debug(logger, "SE ENCONTRARON %d BLOQUES LIBRES A PARTIR DEL BLOQUE INDICADO", cant_bloques);
                    return pos_inicial_buscada;  // devuelve el primer bloque libre para asignar luego
                }
            } else { //1
                log_debug(logger, "NO SE ENCONTRARON %d BLOQUES LIBRES A PARTIR DEL BLOQUE INDICADO", cant_bloques);
                return -1;  // no hay los suficientes bloques libres contiguos al bloque de la pos_inicial_buscada
            }
        }
        log_debug(logger, "NO ES POSIBLE ASIGNAR %d BLOQUES A PARTIR DEL BLOQUE INDICADO, YA QUE PASARIA EL FINAL DEL BITMAP", cant_bloques);
        return -2; // devuelve -1 si no hay suficientes bloques contiguos para el truncate
    }
}
// OK!
int contar_bloques_libres(){
    int bloques_encontrados = 0;
    for (int i = 0; i < config.block_count; i++){
        if (!bitarray_test_bit(bitmap_bloques, i)){
            bloques_encontrados++;
        }
    }
    return bloques_encontrados;
}

//OK!
void cargar_bloque(uint32_t offset, char *contenido){ // reg_puntero, contenido de memoria
    // el reg puntero te dice la direccion del bloque.dat (introducido por param)

    // posiciono el FILE donde quiero escribir dentro del archivo
    fseek(bloques_dat, offset, SEEK_SET); // falta un parametro
    fwrite(contenido, strlen(contenido), 1, bloques_dat);

}

// COMPACTACION
//OK
int primer_bloque_libre(){
    for (int i = 0; i < config.block_count; i++){
        if (!bitarray_test_bit(bitmap_bloques, i))
            return i;
    }
    return -1;
}
//OK
int primer_bloque_usado(int bloque_libre){
    for (int i = bloque_libre; i < config.block_count; i++){
        if (bitarray_test_bit(bitmap_bloques, i))
            return i;
    }
    return -1;
}

void compactar_bloques(uint32_t pid){
    inicio_compactacion(logger, pid);

    int primerBloqueLibre = primer_bloque_libre();
    int primerBloqueUsado;
    // actualizar el bloque inicial de cada archivo que mover

    if (primerBloqueLibre == -1){
        log_error(logger, "No se que queres compactar, todos lo bloques estan leidos.");
        return;
    }

    for (size_t i = primerBloqueLibre; i < config.block_count; i++){
        primerBloqueUsado = primer_bloque_usado(i); // encuentra el primer 1
        
        if (primerBloqueUsado == -1) {
            break;  // No más bloques usados, sal del bucle
        }

        t_archivos_metadata* archivo_usado = buscar_por_bloque_inicial(primerBloqueUsado); // 8

        // necesitas mover el proximo bloque usado a la posiciones de i ( considerando que está libre)
        char *bloque_aux = (char *)malloc(config.block_size);
        // 1. movemos el bloque desde bloques.dat
        fseek(bloques_dat, primerBloqueUsado * config.block_size, SEEK_SET);
        fread(bloque_aux, config.block_size, 1, bloques_dat);
        desasignar_bloque(primerBloqueUsado);  

        // 2. liberar y reasignar bitmap
        fseek(bloques_dat, i * config.block_size, SEEK_SET);
        fwrite(bloque_aux, 1, config.block_size, bloques_dat);
        fflush(bloques_dat);
        asignar_bloque(i);

        // 3. actualizar bloque inicial de archivo si corresponde
        if(archivo_usado){
            archivo_usado->bloque_inicial = i;
            char path[64];
            strcpy(path, config.path_base_dialfs);
            sprintf(path + strlen(path), "/%s", archivo_usado->nombre_archivo_dialfs);

            t_config* archivo_metadata_config = config_create((char*)path);
            char bloque_inicial_nuevo_str[12];
            sprintf(bloque_inicial_nuevo_str, "%ld", i);
            config_set_value(archivo_metadata_config, "BLOQUE_INICIAL", bloque_inicial_nuevo_str);
            config_save(archivo_metadata_config);
            config_destroy(archivo_metadata_config);
        }
        free(bloque_aux);
    }
    fin_compactacion(logger, pid);
}

/*          AUXILIARES            */
void asignar_bloque(int bloque_index) {
    if (bitmap_bloques == NULL || bitmap_bloques->bitarray == NULL) {
        printf("El bitmap no está inicializado correctamente\n");
        return;
    }

    bitarray_set_bit(bitmap_bloques, bloque_index); // cambia bit a 1 

    // esto ya actualiza el bitmap.dat
    if (msync(bitmap_bloques->bitarray, bitmap_bloques->size, MS_SYNC) == -1) {
        printf("Error en la sincronización con msync()\n");
    }
    // ya guardo en el bitmap.dat los valores del bitarray
} 

void desasignar_bloque(int bloque_index){
    if (bitmap_bloques == NULL || bitmap_bloques->bitarray == NULL) {
        printf("El bitmap no está inicializado correctamente\n");
        return;
    }

    bitarray_clean_bit(bitmap_bloques, bloque_index);

    // esto ya actualiza el bitmap.dat
    if (msync(bitmap_bloques->bitarray, bitmap_bloques->size, MS_SYNC) == -1) {
        printf("Error en la sincronización con msync()\n");
    }
    // ya guardo en el bitmap.dat los valores del bitarray
}

t_archivos_metadata* buscar_por_bloque_inicial(int bloque_inicial){ 
    bool comparar_bloque_inicial(void *elemento){
        t_archivos_metadata *archivo = (t_archivos_metadata *)elemento;
        return archivo->bloque_inicial == bloque_inicial;
    }
    t_archivos_metadata* encontrado = (t_archivos_metadata*)list_find(lista_archivos_abierto, comparar_bloque_inicial);
    return encontrado; 
}
