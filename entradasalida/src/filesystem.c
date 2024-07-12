#include <utils/hello.h>
#include <string.h>
#include "filesystem.h"
#include <math.h>
#include "logs.h"

config_struct config;
char *path_bloques;
t_bitarray *bitmap_bloques;
FILE *bloques_dat;
FILE *bitmap_dat;

//     NACIMIENTO

void create_bloques_dat()
{
    char *path = strcat(config.path_base_dialfs, "/bloque.dat");
    bloques_dat = fopen(path, "wb"); // un issue recomendo abrirlo con wb

    if (bloques_dat == NULL)
    {
        log_error(logger, "Error al inicializar bloques.dat");
        exit(EXIT_FAILURE);
    }
}

void init_bitmap_bloques()
{
    size_t tamanio_bitmap = config.block_count;

    char *bitmap = malloc(tamanio_bitmap); // tamanio en bytes que se va a reservar
    if (!bitmap)
    {
        log_error(logger, "Error en la creación del Bitmap de Bloques Libres");
        exit(EXIT_FAILURE);
    }

    bitmap_bloques = bitarray_create_with_mode(bitmap, tamanio_bitmap, LSB_FIRST);

    // inicializa todos los bits a 0 (todos los bloques están libres)
    for (size_t i = 0; i < config.block_count; i++)
    {
        bitarray_clean_bit(bitmap_bloques, i);
    }

    actualizar_bitmap_dat();
}

void fs_create()
{

    create_bloques_dat();
    init_bitmap_bloques();
}

// MUERTE

void close_bloques_dat()
{
    fclose(bloques_dat);
    log_debug(logger, "Archivos bloques.dat cerrado.");
}

void destroy_bitmap_bloques()
{
    bitarray_destroy(bitmap_bloques);
}

// PETICIONES

void io_fs_create(uint32_t pid, char *nombre_archivo)
{
    if (contar_bloques_libres() >= 1)
    {
        char *path = strcat(config.path_base_dialfs, nombre_archivo);

        FILE *archivo_metadata = fopen(path, "w");
        if (archivo_metadata == NULL)
        {
            log_error(logger, "Error al abrir archivo metadata del proceso %d.", pid);
            return;
        }

        int bloque_inicial = buscar_bloques_libres_contiguos(1, -1);
        int valor_cero = 0;

        fwrite(&bloque_inicial, sizeof(int), 1, archivo_metadata); // bloque inicial
        fwrite(&valor_cero, sizeof(int), 1, archivo_metadata);     // tamaño de archivo
        fclose(archivo_metadata);

        bitarray_set_bit(bitmap_bloques, bloque_inicial);
        actualizar_bitmap_dat();
        log_crear_archivo(logger, pid, nombre_archivo);
    }
    else
    {
        log_error(logger, "No hay bloques disponibles en el FS");
    }
}

void io_fs_delete(uint32_t pid, char *nombre_archivo)
{
    int bloque_inicial;
    int tamanio;
    int bloque_final;
    char *path = strcat(config.path_base_dialfs, nombre_archivo);

    FILE *archivo_metadata = fopen(path, "r");
    if (archivo_metadata == NULL)
    {
        log_error(logger, "Error al abrir archivo metadata del proceso %d.", pid);
        return;
    }
    fread(&bloque_inicial, sizeof(int), 1, archivo_metadata);
    fread(&tamanio, sizeof(int), 1, archivo_metadata);

    double division = (double)tamanio / config.block_size;
    bloque_final = bloque_inicial + ceil(division);
    for (int i = bloque_inicial; i < bloque_final; i++)
    {
        bitarray_clean_bit(bitmap_bloques, i);
    }

    fclose(archivo_metadata);
    remove(path);
    log_eliminar_archivo(logger, pid, nombre_archivo);
}

void io_fs_truncate(uint32_t pid, char *nombre_archivo, uint32_t nuevo_tamanio)
{
    int bloque_inicial;
    int bloque_final; // TODO: INICIALIZAR
    int tamanio;

    char *path = strcat(config.path_base_dialfs, nombre_archivo);
    FILE *archivo_metadata = fopen(path, "r");
    if (archivo_metadata == NULL)
    {
        log_error(logger, "Error al abrir archivo metadata del proceso %d.", pid);
        return;
    }
    fread(&bloque_inicial, sizeof(int), 1, archivo_metadata);
    fread(&tamanio, sizeof(int), 1, archivo_metadata);
    fclose(archivo_metadata);

    int cant_bloques = (int)ceil((double)tamanio / config.block_size);
    int nueva_cant_bloques = (int)ceil((double)nuevo_tamanio / config.block_size);

    bloque_final = bloque_inicial + cant_bloques - 1;

    archivo_metadata = fopen(path, "w");
    if (archivo_metadata == NULL)
    {
        log_error(logger, "Error al abrir archivo metadata del proceso %d.", pid);
        return;
    }

    if (cant_bloques < nueva_cant_bloques)
    {
        int cant_a_agregar = cant_bloques - nueva_cant_bloques;

        if (contar_bloques_libres() >= cant_a_agregar)
        {
            int pos_inicial_contigua = buscar_bloques_libres_contiguos(cant_a_agregar, bloque_inicial);
            int pos_inicial_nueva;
            if (pos_inicial_contigua != -1)
            { // agregar a continuacion del bloque final
                for (int i = bloque_inicial; i < bloque_inicial + nueva_cant_bloques; i++)
                {
                    if (!bitarray_test_bit(bitmap_bloques, i))
                    {
                        bitarray_set_bit(bitmap_bloques, i);
                    }
                }
                fwrite(&pos_inicial_contigua, sizeof(int), 1, archivo_metadata);
                fwrite(&nuevo_tamanio, sizeof(uint32_t), 1, archivo_metadata);
                fclose(archivo_metadata);
            }
            else if (pos_inicial_nueva != -1)
            { // busco lugar con n cantidad de bloques contiguos libres
                pos_inicial_nueva = buscar_bloques_libres_contiguos(nueva_cant_bloques, -1);

                char *contenido = malloc(sizeof(uint32_t));

                fseek(bloques_dat, bloque_inicial * config.block_size, SEEK_SET);
                fread(&contenido, sizeof(uint32_t), 1, bloques_dat);

                for (int i = bloque_inicial; i < bloque_inicial + cant_bloques; i++)
                {
                    bitarray_clean_bit(bitmap_bloques, i);
                } // libero bits
                for (int i = pos_inicial_nueva; i < pos_inicial_nueva + nueva_cant_bloques; i++)
                {
                    bitarray_set_bit(bitmap_bloques, i);
                } // asigno nuevos bits
                fseek(bloques_dat, pos_inicial_nueva * config.block_size, SEEK_SET);
                fwrite(&contenido, sizeof(uint32_t), 1, bloques_dat);

                fwrite(&pos_inicial_nueva, sizeof(int), 1, archivo_metadata);
                fwrite(&nuevo_tamanio, sizeof(uint32_t), 1, archivo_metadata);
                fclose(archivo_metadata);

                compactar_bloques();
                free(contenido);
            }
            else
            { // rejuntar todos bloques sueltos y escribir ahi
                // guardar bloques escritos
                // liberar bit de archivo del bitmap
                char *contenido = malloc(sizeof(uint32_t));

                fseek(bloques_dat, bloque_inicial * config.block_size, SEEK_SET);
                fread(&contenido, sizeof(uint32_t), 1, bloques_dat);

                for (int i = bloque_inicial; i < bloque_inicial + cant_bloques; i++)
                {
                    bitarray_clean_bit(bitmap_bloques, i);
                } // libero bits
                compactar_bloques();
                pos_inicial_nueva = buscar_bloques_libres_contiguos(nueva_cant_bloques, -1);
                for (int i = pos_inicial_nueva; i < pos_inicial_nueva + nueva_cant_bloques; i++)
                {
                    bitarray_set_bit(bitmap_bloques, i);
                } // asigno nuevos bits
                fseek(bloques_dat, pos_inicial_nueva * config.block_size, SEEK_SET);
                fwrite(&contenido, sizeof(uint32_t), 1, bloques_dat);

                fwrite(&pos_inicial_nueva, sizeof(int), 1, archivo_metadata);
                fwrite(&nuevo_tamanio, sizeof(int), 1, archivo_metadata);
                free(contenido);
                fclose(archivo_metadata);
            }
        }
        else
        {
            log_error(logger, "No hay mas bloques libres para realizar el truncate");
        }
    }
    else if (cant_bloques > nueva_cant_bloques)
    {
        for (int i = bloque_final; i > (bloque_final - nueva_cant_bloques); i++)
        {
            bitarray_clean_bit(bitmap_bloques, i);
        }
        fwrite(&bloque_inicial, sizeof(int), 1, archivo_metadata);
        fwrite(&nuevo_tamanio, sizeof(uint32_t), 1, archivo_metadata);
        fclose(archivo_metadata);
    }
    actualizar_bitmap_dat();
    log_truncar_archivo(logger, pid, nombre_archivo, nuevo_tamanio);
}

void io_fs_write(uint32_t pid, char *nombre_archivo, uint32_t size, uint32_t pointer, int socket)
{

    int recibir_operacion_memoria = recibir_operacion(socket);
    if (recibir_operacion_memoria == PETICION_LECTURA)
    {
        t_sbuffer *buffer_rta_peticion_lectura = cargar_buffer(socket);
        int cantidad_peticiones = buffer_read_int(buffer_rta_peticion_lectura);

        void *peticion_completa = malloc(size);
        uint32_t bytes_recibidos = 0;
        for (size_t i = 0; i < cantidad_peticiones; i++)
        {
            uint32_t bytes_peticion;
            void *dato_peticion = buffer_read_void(buffer_rta_peticion_lectura, &bytes_peticion);
            memcpy(peticion_completa + bytes_recibidos, dato_peticion, bytes_peticion);
            bytes_recibidos += bytes_peticion;
            free(dato_peticion);
        }

        char *output = malloc(size + 1); // +1 para el carácter nulo al final
        memcpy(output, peticion_completa, size);
        output[size] = '\0'; // YA TENES CARGADO EL VALOR LEIDO DESDE MEMORIA

        { // ESCRITURA EN ARCHIVO!

            char *path = strcat(config.path_base_dialfs, nombre_archivo);
            FILE *archivo_metadata = fopen(path, "r+"); // abrimos el archivo metadata y nos posicionamos al principio
            if (archivo_metadata == NULL)
            {
                log_error(logger, "Error al abrir archivo metadata del proceso %d.", pid);
                return;
            }
            if (check_size(size, pointer, archivo_metadata))
            {
                cargar_bloque(archivo_metadata, pointer, output);
            } // en base al offset me fijo si excede el tamano del archivo
            // size cuanto quiero escrbibir, pointer es desde donde quiero escrbibir,
            //  del metadata saco la posicion inicial y el tam total
            // check que lo que queda en el metadata entre

            fclose(archivo_metadata);
        }

        log_escribir_archivo(logger, pid, nombre_archivo, size, pointer);

        free(output);
        free(peticion_completa);
        buffer_destroy(buffer_rta_peticion_lectura);
    }
}

void io_fs_read(uint32_t pid, char *nombre_archivo, uint32_t size, uint32_t offset, int socket)
{

    char *path = strcat(config.path_base_dialfs, nombre_archivo);
    FILE *archivo_metadata = fopen(path, "r");
    if (archivo_metadata == NULL)
    {
        log_error(logger, "Error al abrir archivo metadata del proceso %d.", pid);
        return;
    }
    fseek(archivo_metadata, offset, SEEK_SET);

    char *datos_leidos = (char *)malloc(size);
    fread(datos_leidos, size, 1, archivo_metadata);
    fclose(archivo_metadata);
    // hacer el buffer

    log_leer_archivo(logger, pid, nombre_archivo, size, offset);
}

// MANEJO BLOQUES Y BITMAP

int buscar_bloques_libres_contiguos(int cant_bloques, int pos_inicial_buscada)
{
    int bloques_encontrados = 0;
    if (pos_inicial_buscada == -1)
    { // Si es -1, entonces no nos importa la posicion de partida.
        for (int i = 0; i < config.block_count; i++)
        {
            if (!bitarray_test_bit(bitmap_bloques, i))
            {
                bloques_encontrados++;
                for (int pos_referencia = i; pos_referencia < (pos_referencia + cant_bloques); pos_referencia++)
                {
                    if (bitarray_test_bit(bitmap_bloques, i))
                    { // si aun no llegamos a la cant buscada, y dio leido, este de referencia no nos sirve.
                        bloques_encontrados = 0;
                        break;
                    }
                    else
                    {
                        bloques_encontrados++;
                    }
                }
                if (bloques_encontrados == cant_bloques)
                    return i; // devolvemos el inicial de la lista de n bloques vacios contiguos.
            }
        }
        return -1; // si llego hasta aca, entonces no se encontraron n bloques contiguos en todo el bitmap
    }
    else
    {
        for (int i = pos_inicial_buscada; i < config.block_count; i++)
        {
            if (!bitarray_test_bit(bitmap_bloques, i))
            {
                bloques_encontrados++;
            }
            if (bloques_encontrados == cant_bloques)
                return pos_inicial_buscada;
        }
        return -1; // devuelve -1 si no hay suficientes bloques contiguos para el truncate
    }
}

int contar_bloques_libres()
{
    int bloques_encontrados = 0;
    for (int i = 0; i < config.block_count; i++)
    {
        if (!bitarray_test_bit(bitmap_bloques, i))
        {
            bloques_encontrados++;
        }
    }
    return bloques_encontrados;
}

void cargar_bloque(FILE *archivo_metadata, uint32_t offset, char *contenido)
{ // reg_puntero, contenido de memoria
    // el reg puntero te dice la direccion del metadata (introducido por param)

    // en base a la direccion del metadata, se obtiene el bloque inicial
    uint32_t bloque_inicial;
    fread(&bloque_inicial, sizeof(uint32_t), 1, archivo_metadata);

    // posiciono el FILE donde quiero escribir dentro del archivo
    fseek(bloques_dat, bloque_inicial * config.block_size + offset, SEEK_SET); // falta un parametro

    uint32_t cantidad_bloques = ceil(sizeof(contenido) / config.block_size);

    // escribo
    size_t elementos_escritos = fwrite(contenido, config.block_size, cantidad_bloques, bloques_dat); // esto puede excederse del archivo, pero está confiando de que no puede llegar un archivo que exceda por el checkeo
    // chequear que el bloque se haya cargado bien
    if (elementos_escritos < cantidad_bloques)
    {
        log_error(logger, "Error al cargar bloque.")
            exit(EXIT_FAILURE);
    }
}

// COMPACTACION

int primer_bloque_libre()
{
    for (int i = 0; i < config.block_count; i++)
    {
        if (bitarray_test_bit(bitmap_bloques, i))
            return i;
    }
    return -1;
}

int primer_bloque_usado()
{
    for (int i = 0; i < config.block_count; i++)
    {
        if (!bitarray_test_bit(bitmap_bloques, i))
            return i;
    }
    return -1;
}

void compactar_bloques(uint32_t pid)
{
    inicio_compactacion(logger, pid);

    int primerBloqueLibre = primer_bloque_libre();
    int primerBloqueUsado = primer_bloque_usado();
    char *bloque_aux = (char *)malloc(config.block_size);

    if (primerBloqueLibre == -1 || primerBloqueUsado == -1)
    {
        log_error(logger, "No se que queres compactar, todos lo bloques estan leidos.");
        return;
    }

    while (primerBloqueLibre < config.block_count && primerBloqueUsado < config.block_count)
    {
        if (primerBloqueLibre < primerBloqueUsado)
        {
            fseek(bloques_dat, primerBloqueUsado * config.block_size, SEEK_SET);
            fread(bloque_aux, 1, config.block_size, bloques_dat);

            fseek(bloques_dat, primerBloqueLibre * config.block_size, SEEK_SET);
            fwrite(bloque_aux, 1, config.block_size, bloques_dat);

            bitarray_set_bit(bitmap_bloques, primerBloqueLibre);
            bitarray_clean_bit(bitmap_bloques, primerBloqueUsado);

            free(bloque_aux);

            primerBloqueLibre = primer_bloque_libre();
            primerBloqueUsado = primer_bloque_usado();
        }
        else
        {
            primerBloqueLibre++;
            primerBloqueUsado++;
        }
    }
    actualizar_bitmap_dat();
    fin_compactacion(logger, pid);
}

/*          AUXILIARES            */

void actualizar_bitmap_dat()
{
    char path[512]; // Asumimos que 512 es suficiente, ajusta según necesites
    snprintf(path, sizeof(path), "%s/bitmap.dat", config.path_base_dialfs);
    bitmap_dat = fopen(path, "wb"); // un issue recomendo abrirlo con wb

    for (size_t i = 0; i < config.block_count; i++)
    {
        bool bit = bitarray_test_bit(bitmap_bloques, i);
        // uint8_t bit_guardar = bit;
        // char bit_char = bit + '0';
        fwrite(&bit, sizeof(bool), 1, bitmap_dat);
    }

    fclose(bitmap_dat);
}

bool check_size(uint32_t size, uint32_t offset, FILE *archivo_metadata){

    long posicion_actual = ftell(archivo_metadata); //me guardo la posicion actual que trajo el archivo

    uint32_t tamanio_archivo;
    fseek(archivo_metadata, 0, SEEK_SET); //me muevo al inicio del archivo por las dudas!!
    fread(&tamanio_archivo, sizeof(uint32_t), 1, archivo_metadata);
    
    if (offset > tamanio_archivo) {
        log_error(logger,"El offset que pasaste es mas granque que el archivo, idiota!");
        return EXIT_FAILURE;
    }

    uint32_t bytes_leidos = offset;
    uint32_t bytes_restantes = tamanio_archivo - bytes_leidos;

    fseek(archivo_metadata, posicion_actual, SEEK_SET); //pongo el puntero del archivo donde estaba

    return (bytes_restantes>=size);
}