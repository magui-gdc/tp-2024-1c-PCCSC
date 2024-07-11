#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

extern t_config* config;

//TODO:::::: CHECKEAR QUE LOS MALLOCS SE LIBEREN!!!!!!!!!!

int main(int argc, char* argv[]) {
    
    int conexion_kernel, conexion_memoria;
    char* nombre = obtener_path(); 
    char archivo_log[64];
    char archivo_configuracion[64];
    strcpy(archivo_configuracion, nombre);
    strcat(archivo_configuracion, ".config");
    strcpy(archivo_log, nombre);
    strcat(archivo_log, ".log");
    t_config* archivo_config = config_create(archivo_configuracion);
    logger = log_create(archivo_log, "Interfaz I/O", 0, LOG_LEVEL_DEBUG);
    decir_hola("una Interfaz de Entrada/Salida");
    
     //----- RECIBO ARCHIVO CONFIG E INICIALIZO LA IO
    t_io* interfaz_io = inicializar_io(nombre, archivo_config); //TODO: free(interfaz_io)
    if(interfaz_io->clase == FS){
        fs_create();
    }

    // EN ESTE PUNTO LA IO CUENTA CON UN CONFIG CARGADO Y UNA INTERFAZ QUE CONTIENE TANTO EL ARCHIVO CONFIG, COMO SU NOMBRE Y LA CLASE

    /////////////////// CONEXIONES //////////////////////

    // establecer conexion con KERNEL
    conexion_kernel = crear_conexion(config.ip_kernel, config.puerto_kernel);
   
    //----- IDENTIFICO LA IO CON EL KERNEL
    
    t_sbuffer* buffer_conexion = buffer_create(
        (uint32_t)strlen(interfaz_io->nombre_id) + sizeof(uint32_t) // tamanio del string
        + (uint32_t)strlen(config.tipo_interfaz) + sizeof(uint32_t) // + sumar todos los sizeof de los valores que le vas a pasar (tipo_interfaz, path , ...)
    );

    buffer_add_string(buffer_conexion, (uint32_t)strlen(interfaz_io->nombre_id), interfaz_io->nombre_id);
    buffer_add_string(buffer_conexion, (uint32_t)strlen(config.tipo_interfaz), config.tipo_interfaz);
    cargar_paquete(conexion_kernel, CONEXION, buffer_conexion);
    log_info(logger, "Envie conexion a KERNEL");
    
    // establecer conexion con MEMORIA
    if(interfaz_io->clase != GEN){
        conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);
        log_debug(logger, "Envie conexion a MEMORIA");
    }


    sem_t mutex_config;
    sem_init(&mutex_config, 0, 1); //TODO: destruir semaforo/s

    ////////////////////////////////////////////////////////
    //TODO: desarrollar funciones según tipo
    
    //TODO: esperar que reciba comunicacion del kernel y verificar que el codigo de abajo este bien xd

    //TODO: free() punteros

    // un ciclo para esperar constantemente indicaciones del lado KERNEL
    while(1){

        // 1. siempre ejecuta recibir_operación antes de cargar el buffer ya que recibir_operación hace un RECV bloqueante y lee la operación que es LO PRIMERO que se manda en un paquete, luego el buffer en sí 
        int op = recibir_operacion(conexion_kernel); // DE MEMORIA SIEMPRE ENVIA A DEMANDA A PARTIR DE LAS PETICIONES DE KERNEL POR LO QUE NO HACE FALTA CICLO 
        
        // 2. Se inicializa el buffer. Se lee lo que haya en el buffer desde cada operación ya que conociendo la operacion sabes qué tenes que recibir/leer
        t_sbuffer* buffer_operacion = cargar_buffer(conexion_kernel); 

        // 3. Se procesa la operación
        
        switch (interfaz_io->clase){
        case GEN:
            if(op == IO_GEN_SLEEP){
                // A. ACA RECIBE EL RESTO DEL CONTENIDO DEL BUFFER PORQUE SABE LO QUE DEBERÍA RECIBIR 
                uint32_t proceso = buffer_read_uint32(buffer_operacion);
                log_info(logger, "PID: %u - Operacion: IO_GEN_SLEEP", proceso);

                // EN ESE CASO RECIBIRÍA UN NÚMERO PARA HACER LAS ITERACIONES
                uint32_t iteraciones = buffer_read_uint32(buffer_operacion);
                log_debug(logger, "se manda a dormir %u iteraciones", iteraciones);

                // B. Ejecutar instrucción!
                io_gen_sleep(iteraciones);

                // C. Responder a KERNEL
                responder_kernel(conexion_kernel);
            }
            break;
        case IN:
            if(op == IO_STDIN_READ){
                uint32_t bytes_a_leer_desde_consola = buffer_read_uint32(buffer_operacion);
                buffer_read_uint32(buffer_operacion); // LEER EL TAMANIO DEL BUFFER POR COMO FUE CARGADO BUFFER MMU EN CPU

                io_stdin_read(buffer_operacion, bytes_a_leer_desde_consola, conexion_memoria);

                responder_kernel(conexion_kernel);
            }
            break;
        case OUT:
            if(op == IO_STDOUT_WRITE){
                uint32_t bytes_a_leer_desde_memoria = buffer_read_uint32(buffer_operacion);
                buffer_read_uint32(buffer_operacion); // LEER EL TAMANIO DEL BUFFER POR COMO FUE CARGADO BUFFER MMU EN CPU

                io_stdout_write(buffer_operacion, bytes_a_leer_desde_memoria, conexion_memoria);

                responder_kernel(conexion_kernel);
            }
            break;
        case FS:
            if(op == IO_FS_CREATE){
                uint32_t proceso = buffer_read_uint32(buffer_operacion);
                uint32_t length_file;
                char* nombre_file = buffer_read_string(buffer_operacion, &length_file);

                io_fs_create(proceso, nombre_file, conexion_memoria);

                responder_kernel(conexion_kernel);
            }
            if(op == IO_FS_DELETE){
                uint32_t proceso = buffer_read_uint32(buffer_operacion);
                uint32_t length_file;
                char* nombre_file = buffer_read_string(buffer_operacion, &length_file);

                io_fs_delete(proceso, nombre_file, conexion_memoria);

                responder_kernel(conexion_kernel);
                
            }
            if(op == IO_FS_TRUNCATE){
                uint32_t proceso = buffer_read_uint32(buffer_operacion);
                uint32_t length_file;
                char* nombre_file = buffer_read_string(buffer_operacion, &length_file);
                uint32_t tamanio_truncate = buffer_read_uint32(buffer_operacion);



                responder_kernel(conexion_kernel);
            }
            if(op == IO_FS_WRITE){
                // todo: se le habla a memoria
                uint32_t length_file;
                char* nombre_file = buffer_read_string(buffer_operacion, &length_file);
                uint32_t bytes_a_leer_desde_memoria = buffer_read_uint32(buffer_operacion);
                uint32_t offset_puntero_archivo = buffer_read_uint32(buffer_operacion);
                buffer_read_uint32(buffer_operacion); // LEER EL TAMANIO DEL BUFFER POR COMO FUE CARGADO BUFFER MMU EN CPU
                
                uint32_t proceso = buffer_read_uint32(buffer_operacion);
                log_info(logger, "PID: %u - Operacion: IO_STDOUT_WRITE", proceso);

                log_debug(logger, "bytes a leer desde memoria %u", bytes_a_leer_desde_memoria);

                buffer_operacion->offset-=sizeof(uint32_t); // retrocedo offset para que envíe el PID del proceso a memoria
                uint32_t nuevo_tamanio = buffer_operacion->size - sizeof(uint32_t)*2; // le resto tamanio del size y del buffer mmu anterior que no se debe enviar a memoria
                t_sbuffer* buffer_memoria = buffer_create(nuevo_tamanio);
                buffer_read(buffer_operacion, buffer_memoria->stream, nuevo_tamanio); // el resto del buffer lo copia en el nuevo buffer  

                cargar_paquete(conexion_memoria, PETICION_LECTURA, buffer_memoria);
                
                io_fs_write(proceso, nombre_file, bytes_a_leer_desde_memoria, offset_puntero_archivo, conexion_memoria)
                
                buffer_destroy(buffer_memoria);

                responder_kernel(conexion_kernel);
            }
            if(op == IO_FS_READ){
                uint32_t length_file;
                char* nombre_file = buffer_read_string(buffer_operacion, &length_file);
                uint32_t bytes_a_leer_desde_archivo = buffer_read_uint32(buffer_operacion);
                uint32_t offset_puntero_archivo = buffer_read_uint32(buffer_operacion);
                buffer_read_uint32(buffer_operacion); // LEER EL TAMANIO DEL BUFFER POR COMO FUE CARGADO BUFFER MMU EN CPU
                
                uint32_t proceso = buffer_read_uint32(buffer_operacion);
                log_info(logger, "PID: %u - Operacion: IO_STDOUT_WRITE", proceso);

                log_debug(logger, "bytes a leer desde archivo %u", bytes_a_leer_desde_archivo);

                char* dato_a_leer_desde_archivo = malloc(bytes_a_leer_desde_archivo + 1); // dato + /0

                io_fs_write(proceso, nombre_file, bytes_a_leer_desde_archivo, offset_puntero_archivo, conexion_memoria)

                int cantidad_peticiones_memoria = buffer_read_int(buffer_operacion);
                uint32_t tamanio_buffer_memoria = sizeof(uint32_t) + // pid proceso!
                    sizeof(int) + // cantidad de peticiones
                    sizeof(uint32_t) * cantidad_peticiones_memoria + // direccion/es fisica/s = nro_marco * tam_pagina + desplazamiento 
                    sizeof(uint32_t) * cantidad_peticiones_memoria + bytes_a_leer_desde_archivo; // bytes de escritura por petición void*

                t_sbuffer* buffer_memoria = buffer_create(tamanio_buffer_memoria);
                buffer_add_uint32(buffer_memoria, proceso);
                buffer_add_int(buffer_memoria, cantidad_peticiones_memoria);

                void* dato_enviar = dato_a_leer_desde_archivo;
                uint32_t bytes_enviados = 0;
                log_debug(logger, "cantidad peticiones IO %d", cantidad_peticiones_memoria);
                for (size_t i = 0; i < cantidad_peticiones_memoria; i++){
                    uint32_t dir_fisica = buffer_read_uint32(buffer_operacion);
                    buffer_add_uint32(buffer_memoria, dir_fisica); // agrega direccion fisica
                    uint32_t tamanio_peticion = buffer_read_uint32(buffer_operacion);
                    log_debug(logger, "dir. fisica %u, tamanio peticion %u", dir_fisica, tamanio_peticion);
                    void* datos_a_enviar = malloc(tamanio_peticion);
                    memcpy(datos_a_enviar, dato_enviar + bytes_enviados, tamanio_peticion);
                    buffer_add_void(buffer_memoria, datos_a_enviar, tamanio_peticion); // agrega void* con su respectivo tamanio

                    free(datos_a_enviar); // para la próxima petición
                    bytes_enviados+=tamanio_peticion;
                }

                cargar_paquete(conexion_memoria, PETICION_ESCRITURA, buffer_memoria);
                recibir_operacion(conexion_memoria);
                
                buffer_destroy(buffer_memoria)

                responder_kernel(conexion_kernel);
            }
            break;
        default:
            break;
        }


        // Liberar memoria
        buffer_destroy(buffer_operacion); // liberar SIEMPRE el buffer después de usarlo
    }


    ///////////////////////// GENERICA /////////////////////////



    ///////////////////////// STDIN    /////////////////////////



    //////////////////////// STDOUT    /////////////////////////



    ///////////////////////// DIALFS   /////////////////////////
 

    ///////////////////////////////////////////////////
    log_destroy(logger);
    free(nombre);
	config_destroy(archivo_config);
	liberar_conexion(conexion_kernel);
    liberar_conexion(conexion_memoria);

    return 0;
}

void responder_kernel(int socket){
    op_code respuesta_kernel = IO_LIBERAR; // si todo fue ok SIEMPRE enviar IO_LIBERAR
    ssize_t bytes_enviados = send(socket, &respuesta_kernel, sizeof(respuesta_kernel), 0);
    if (bytes_enviados == -1) {
        log_error(logger, "Error enviando el dato");
        exit(EXIT_FAILURE);
    }
}

////////////// CONFIGURACIÓN DEL CONFIG Y DERIVADOS ////////////////////////

t_io* inicializar_io(char* nombre, t_config* archivo_config){
    t_io* interfaz = malloc(sizeof(t_io)); //TODO: free(interfaz)
    interfaz->nombre_id = nombre;
    interfaz->archivo = archivo_config;
    interfaz->clase = selector_carga_config(archivo_config);
    
    return interfaz;
}

IO_class selector_carga_config (t_config* archivo_config){

    char* tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    IO_class clase;
    if (strcmp(tipo_interfaz, "GENERICA") == 0) {
        cargar_config_struct_IO_gen(archivo_config);
        clase = GEN;
    } else if (strcmp(tipo_interfaz, "STDIN") == 0) {
        cargar_config_struct_IO_in(archivo_config);
        clase = IN;
    } else if (strcmp(tipo_interfaz, "STDOUT") == 0) {
        cargar_config_struct_IO_out(archivo_config);
        clase = OUT;
    } else if (strcmp(tipo_interfaz, "DIALFS") == 0) {
        cargar_config_struct_IO_fs(archivo_config);
        clase = FS;
    } else {
        log_error(logger, "interfaz no reconocida XD: %s", tipo_interfaz);
        clase = MISSING;
    }
    return clase;
}

char* obtener_path(){
    char* path = malloc(64);
    while (strlen(path) == 0) {
        printf("Introducir el nombre de la IO: ");
        scanf("%s", path);
    }
    return path;
}


//-----------------CARGAR CONFIG SEGUN DISPOSITIVO IO-----------------//

///////////////////////// GENÉRICA /////////////////////////

void cargar_config_struct_IO_gen(t_config* archivo_config){
    config.tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    config.tiempo_unidad_trabajo = config_get_string_value(archivo_config, "TIEMPO_UNIDAD_TRABAJO");
    config.ip_kernel = config_get_string_value(archivo_config, "IP_KERNEL");
    config.puerto_kernel = config_get_string_value(archivo_config, "PUERTO_KERNEL");
}

///////////////////////// STDIN    /////////////////////////

void cargar_config_struct_IO_in(t_config* archivo_config){
    config.tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    config.ip_kernel = config_get_string_value(archivo_config, "IP_KERNEL");
    config.puerto_kernel = config_get_string_value(archivo_config, "PUERTO_KERNEL");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
}

//////////////////////// STDOUT    /////////////////////////

void cargar_config_struct_IO_out(t_config* archivo_config){
    config.tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    config.tiempo_unidad_trabajo = config_get_string_value(archivo_config, "TIEMPO_UNIDAD_TRABAJO");
    config.ip_kernel = config_get_string_value(archivo_config, "IP_KERNEL");
    config.puerto_kernel = config_get_string_value(archivo_config, "PUERTO_KERNEL");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
}

///////////////////////// DIALFS   /////////////////////////

void cargar_config_struct_IO_fs(t_config* archivo_config){
    config.tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    config.tiempo_unidad_trabajo = config_get_string_value(archivo_config, "TIEMPO_UNIDAD_TRABAJO");
    config.ip_kernel = config_get_string_value(archivo_config, "IP_KERNEL");
    config.puerto_kernel = config_get_string_value(archivo_config, "PUERTO_KERNEL");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
    config.path_base_dialfs = config_get_string_value(archivo_config, "PATH_BASE_DIALFS");
    config.block_size = config_get_string_value(archivo_config, "BLOCK_SIZE");
    config.block_count = config_get_string_value(archivo_config, "BLOCK_COUNT");
    config.block_count = config_get_string_value(archivo_config, "RETRASO_COMPACTACION");
}

//-----------------FUNCIONES DE IO-----------------//

    ///////////////////////// GENERICA /////////////////////////

void io_gen_sleep(uint32_t iteraciones){
    char* tiempo = config.tiempo_unidad_trabajo; 
    usleep(atoi(tiempo) * iteraciones * 1000); // EL TIEMPO UNIDAD TRABAJO ESTÁ EXPRESADO EN MS => usar usleep que trabaja con microsegundos! 
}

    ///////////////////////// STDIN    /////////////////////////

void io_stdin_read(t_sbuffer* direcciones_memoria, uint32_t size, int socket){
    uint32_t proceso = buffer_read_uint32(direcciones_memoria);
    log_info(logger, "PID: %u - Operacion: IO_STDIN_READ", proceso);
    log_debug(logger, "bytes a leer desde consola %u", size);

    // CARGO VALOR
    uint32_t bytes_lectura = size + 1; // un espacio para el /0
    char* input = malloc(bytes_lectura);

    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    printf("Ingresar texto para guardar en memoria:\n");

    if (fgets(input, bytes_lectura, stdin) != NULL) {
        input[strcspn(input, "\n")] = '\0';
        printf("se ingreso: %s\n", input);

        int cantidad_peticiones_memoria = buffer_read_int(direcciones_memoria);
        uint32_t tamanio_buffer_memoria = sizeof(uint32_t) + // pid proceso!
            sizeof(int) + // cantidad de peticiones
            sizeof(uint32_t) * cantidad_peticiones_memoria + // direccion/es fisica/s = nro_marco * tam_pagina + desplazamiento 
            sizeof(uint32_t) * cantidad_peticiones_memoria + size; // bytes de escritura por petición void*

        t_sbuffer* buffer_memoria = buffer_create(tamanio_buffer_memoria);
        buffer_add_uint32(buffer_memoria, proceso);
        buffer_add_int(buffer_memoria, cantidad_peticiones_memoria);

        void* dato_enviar = input;
        uint32_t bytes_enviados = 0;
        log_debug(logger, "cantidad peticiones IO %d", cantidad_peticiones_memoria);
        for (size_t i = 0; i < cantidad_peticiones_memoria; i++){
            uint32_t dir_fisica = buffer_read_uint32(direcciones_memoria);
            buffer_add_uint32(buffer_memoria, dir_fisica); // agrega direccion fisica
            uint32_t tamanio_peticion = buffer_read_uint32(direcciones_memoria);
            log_debug(logger, "dir. fisica %u, tamanio peticion %u", dir_fisica, tamanio_peticion);
            void* datos_a_enviar = malloc(tamanio_peticion);
            memcpy(datos_a_enviar, dato_enviar + bytes_enviados, tamanio_peticion);
            buffer_add_void(buffer_memoria, datos_a_enviar, tamanio_peticion); // agrega void* con su respectivo tamanio

            free(datos_a_enviar); // para la próxima petición
            bytes_enviados+=tamanio_peticion;
        }

        cargar_paquete(socket, PETICION_ESCRITURA, buffer_memoria);
        recibir_operacion(socket);
    } else {
        printf("Error al leer la entrada.\n");
    }
}

    //////////////////////// STDOUT    /////////////////////////

void io_stdout_write(t_sbuffer* direcciones_memoria, uint32_t size, int socket){
    uint32_t proceso = buffer_read_uint32(direcciones_memoria);
    log_info(logger, "PID: %u - Operacion: IO_STDOUT_WRITE", proceso);
    log_debug(logger, "bytes a leer desde consola %u", size);

    direcciones_memoria->offset-=sizeof(uint32_t); // retrocedo offset para que envíe el PID del proceso a memoria
    uint32_t nuevo_tamanio = direcciones_memoria->size - sizeof(uint32_t)*2; // le resto tamanio del size y del buffer mmu anterior que no se debe enviar a memoria
    t_sbuffer* buffer_memoria = buffer_create(nuevo_tamanio);
    buffer_read(direcciones_memoria, buffer_memoria->stream, nuevo_tamanio); // el resto del buffer lo copia en el nuevo buffer  

    cargar_paquete(socket, PETICION_LECTURA, buffer_memoria);
    int recibir_operacion_memoria = recibir_operacion(socket);
    if(recibir_operacion_memoria == PETICION_LECTURA){
        t_sbuffer* buffer_rta_peticion_lectura = cargar_buffer(socket);
        int cantidad_peticiones = buffer_read_int(buffer_rta_peticion_lectura);

        void* peticion_completa = malloc(size);
        uint32_t bytes_recibidos = 0;
        for (size_t i = 0; i < cantidad_peticiones; i++){
            uint32_t bytes_peticion;
            void* dato_peticion = buffer_read_void(buffer_rta_peticion_lectura, &bytes_peticion);
            memcpy(peticion_completa + bytes_recibidos, dato_peticion, bytes_peticion);
            bytes_recibidos += bytes_peticion;
            free(dato_peticion);
        }

        char* valor_leido = malloc(size + 1); // +1 para el carácter nulo al final
        memcpy(valor_leido, peticion_completa, size);
        valor_leido[size] = '\0';

        printf("Dato leido desde memoria %s.\n", valor_leido);

        free(valor_leido);
        free(peticion_completa);
        buffer_destroy(buffer_rta_peticion_lectura);

    }
}


// definirla para que no lance error la compilación por UTILSERVER.C
void* atender_cliente(void* cliente) {
    return NULL;
}