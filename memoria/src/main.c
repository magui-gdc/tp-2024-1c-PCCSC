#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "paginacion.h" 
#include "main.h"

pthread_t thread_memoria;

int main(int argc, char *argv[]){

    // ------ INICIALIZACIÓN VARIABLES GLOBALES ------ //
    t_config *archivo_config = iniciar_config("memoria.config");
    cargar_config_struct_MEMORIA(archivo_config);
    logger = log_create("memoria.log", "Servidor Memoria", 1, LOG_LEVEL_DEBUG);
    decir_hola("Memoria");
    
    init_memoria(); // inicializa la void* memoria y las estructuras necesarias para la paginacion

    // ------ INICIALIZACIÓN SERVIDOR + HILO ESCUCHA ------ //
    int socket_servidor = iniciar_servidor(config.puerto_escucha);
    log_info(logger, config.puerto_escucha);
    log_info(logger, "Server MEMORIA iniciado");

    if (pthread_create(&thread_memoria, NULL, servidor_escucha, &socket_servidor) != 0){
        log_error(logger, "No se ha podido crear el hilo SERVIDOR de MEMORIA");
        exit(EXIT_FAILURE);
    }

    pthread_join(thread_memoria, NULL);


    log_destroy(logger);
    config_destroy(archivo_config);
    bitarray_destroy(bitmap_marcos);

    return EXIT_SUCCESS;
}

void cargar_config_struct_MEMORIA(t_config *archivo_config){
    config.puerto_escucha = config_get_string_value(archivo_config, "PUERTO_ESCUCHA");
    config.tam_memoria = config_get_int_value(archivo_config, "TAM_MEMORIA");
    config.tam_pagina = config_get_int_value(archivo_config, "TAM_PAGINA");
    config.path_instrucciones = config_get_string_value(archivo_config, "PATH_INSTRUCCIONES");
    config.retardo_respuesta = config_get_int_value(archivo_config, "RETARDO_RESPUESTA");
}

void *atender_cliente(void *cliente){
    int cliente_recibido = *(int *)cliente;
    t_temporal* timer; // cada cliente tendrá un timer asociado para controlar el retardo de las peticiones
    while (1){
        int cod_op = recibir_operacion(cliente_recibido); // bloqueante
        switch (cod_op){
        case CONEXION:
            recibir_conexion(cliente_recibido);
        break;
        case DATOS_MEMORIA:
            t_sbuffer* buffer_a_cpu_datos_memoria = buffer_create(
                sizeof(int) * 2
            );
            buffer_add_int(buffer_a_cpu_datos_memoria, config.tam_memoria);
            buffer_add_int(buffer_a_cpu_datos_memoria, config.tam_pagina);
            cargar_paquete(cliente_recibido, DATOS_MEMORIA, buffer_a_cpu_datos_memoria);
        break;
        case INICIAR_PROCESO: // KERNEL
            timer = temporal_create(); // SIEMPRE PRIMERO ESTO! calcula tiempo de petición
            t_sbuffer *buffer_path = cargar_buffer(cliente_recibido);
            uint32_t pid_iniciar = buffer_read_uint32(buffer_path);
            uint32_t longitud_path;
            char* path  = buffer_read_string(buffer_path, &longitud_path);
            path[strcspn(path, "\n")] = '\0'; // CORREGIR: DEBE SER UN PROBLEMA DESDE EL ENVÍO DEL BUFFER!

            crear_proceso(pid_iniciar, path, longitud_path); 

            op_code respuesta_kernel = CONTINUAR; // si todo fue ok enviar CONTINUAR para que kernel pueda continuar con su planificacion
            tiempo_espera_retardo(timer); // ANTES DE RESPONDER ESPERO QUE FINALICE EL RETARDO
            send(cliente_recibido, &respuesta_kernel, sizeof(respuesta_kernel), 0);

            buffer_destroy(buffer_path);
        break;
        case ELIMINAR_PROCESO: // KERNEL
            timer = temporal_create();
            t_sbuffer* buffer_eliminar = cargar_buffer(cliente_recibido);
            uint32_t pid_eliminar = buffer_read_uint32(buffer_eliminar);

            log_debug(logger, "se mando a eliminar/liberar proceso %u", pid_eliminar);
            eliminar_proceso(pid_eliminar); 

            op_code respuesta_cpu = CONTINUAR;
            tiempo_espera_retardo(timer);
            send(cliente_recibido, &respuesta_cpu, sizeof(respuesta_cpu), 0);

            buffer_destroy(buffer_eliminar);
        break;
        case LEER_PROCESO: // CPU
            // 1. comienza a contar el timer para calcular retardo en rta a CPU
            timer = temporal_create();

            // 2. cargo buffer
            t_sbuffer *buffer_lectura = cargar_buffer(cliente_recibido);
            uint32_t pid_proceso = buffer_read_uint32(buffer_lectura); // guardo PID del proceso del cual se quiere leer
            uint32_t pc_proceso = buffer_read_uint32(buffer_lectura);  // guardo PC para elegir la prox INSTRUCCION a ejecutar

            log_debug(logger, "Recibi para leer del proceso %u, la prox INST desde PC %u", pid_proceso, pc_proceso);

            t_pcb* proceso_memoria = get_element_from_pid(pid_proceso);
            if(!proceso_memoria) {
                log_error(logger, "no existe dicho proceso en el pcb de memoria");
                break;
            }
            char* path_instrucciones_proceso = malloc(strlen(config.path_instrucciones) + strlen(proceso_memoria->path_proceso) + 1);
            strcpy(path_instrucciones_proceso, config.path_instrucciones);
            strcat(path_instrucciones_proceso, proceso_memoria->path_proceso);

            log_debug(logger, "este es el path absoluto del proceso %s", path_instrucciones_proceso);
            int lineaActual = 0, lee_instruccion = 0;

            char *instruccion = NULL;
            size_t len = 0;
            FILE *script = fopen(path_instrucciones_proceso, "r");

            if (script == NULL){
                log_error(logger, "No se encontro ningun archivo con el nombre indicado...");
                temporal_destroy(timer);
            } else {
                while (getline(&instruccion, &len, script) != -1){
                    if (lineaActual == pc_proceso){
                        log_debug(logger, "INSTRUCCION LEIDA EN LINEA %d: %s", pc_proceso, instruccion);
                        lee_instruccion = 1;
                        break;
                    }
                    lineaActual++;
                }
                fclose(script);
                
                if(lee_instruccion){
                    t_sbuffer *buffer_instruccion = buffer_create(
                        (uint32_t)strlen(instruccion) + sizeof(uint32_t)
                    );

                    buffer_add_string(buffer_instruccion, (uint32_t)strlen(instruccion), instruccion);

                    tiempo_espera_retardo(timer); // SIEMPRE, antes de terminar la petición a memoria, espera a que se complete el retardo de la configuración
                    cargar_paquete(cliente_recibido, INSTRUCCION, buffer_instruccion);
                    buffer_destroy(buffer_lectura);
                } // else TODO: llega al final y CPU todavía no desalojó el proceso por EXIT (esto pasaría sólo si el proceso no termina con EXIT)
                free(instruccion);
            }
            free(path_instrucciones_proceso);
        break;
        case RESIZE: // solo CPU
            timer = temporal_create(); // SIEMPRE PRIMERO ESTO! calcula tiempo de petición
            t_sbuffer *buffer_resize = cargar_buffer(cliente_recibido);
            uint32_t proceso_resize = buffer_read_uint32(buffer_resize);
            int tamanio_resize = buffer_read_int(buffer_resize);
            resize_proceso(timer, cliente_recibido, proceso_resize, tamanio_resize); // ya responde a CPU y se encarga terminar el retardo
            buffer_destroy(buffer_resize);
        break;
        case TLB_MISS: // CPU
            timer = temporal_create();
            t_sbuffer* buffer_tlb_miss = cargar_buffer(cliente_recibido);
            uint32_t proceso_tlb_miss = buffer_read_uint32(buffer_tlb_miss);
            int pagina = buffer_read_int(buffer_tlb_miss);

            uint32_t marco = obtener_marco_proceso(proceso_tlb_miss, pagina);

            t_sbuffer* buffer_marco_solicitado = buffer_create(sizeof(uint32_t));
            buffer_add_uint32(buffer_marco_solicitado, marco);
            tiempo_espera_retardo(timer);
            cargar_paquete(cliente_recibido, MARCO_SOLICITADO, buffer_marco_solicitado);

            buffer_destroy(buffer_tlb_miss);
        break;
        case PETICION_ESCRITURA: // CPU / IO
        /*
         timer = temporal_create();
        for: cantidad peticiones... (va a acceder a las direcciones fisicas y escribir el dato pasado por parámetro)
        log_acceso_espacio_usuario
        
        tiempo_espera_retardo(timer);
        responder al cliente informando OK o error SIEMPRE RESPONDER
        liberar memoria utilizada (buffers cargados, mallocs, etc)
        */
        break;
        case PETICION_LECTURA: // CPU / IO
        /*
         timer = temporal_create();
        for: cantidad peticiones... (va a acceder a las direcciones fisicas, leer el dato pasado por parámetro y cargar un buffer con cada dato solicitado y su tamanio)
        log_acceso_espacio_usuario

         tiempo_espera_retardo(timer);
        responder al cliente informando OK o error SIEMPRE RESPONDER
        liberar memoria utilizada (buffers cargados, mallocs, etc)
        */
        break;
        case -1:
            log_error(logger, "Cliente desconectado.");
            close(cliente_recibido); // cierro el socket accept del cliente
            free(cliente);           // libero el malloc reservado para el cliente
            pthread_exit(NULL);      // solo sale del hilo actual => deja de ejecutar la función atender_cliente que lo llamó
        break;
        default:
            log_warning(logger, "Operacion desconocida.");
        break;
        }
    }
}


////// funciones memoria
// RESIZE

void resize_proceso(t_temporal* timer, int socket_cliente, uint32_t pid, int new_size){

    // 1. Primera validación: el resize NO es mayor al tam_memoria => si fuese mayor se estaría pidiendo una cantidad de páginas MAYOR a la que puede tener cada tabla de páginas
    if(!suficiente_memoria(new_size)){
        op_code respuesta_cpu = OUT_OF_MEMORY;
        tiempo_espera_retardo(timer);
        ssize_t bytes_enviados = send(socket_cliente, &respuesta_cpu, sizeof(respuesta_cpu), 0);
        if (bytes_enviados == -1) {
            log_error(logger, "Error enviando dato OUT_OF_MEMORY a CPU");
            exit(EXIT_FAILURE);
        }
        return;
    }
    
    // 2. Analizar si corresponde ampliar o reducir el proceso
    int cantidad_paginas_ocupadas = cant_paginas_ocupadas_proceso(pid); // paginas ocupadas por el proceso
    int cantidad_paginas_solicitadas = (int)ceil((double)(new_size / config.tam_pagina)); // paginas que se necesitan con el nuevo valor del resize
    
    if (cantidad_paginas_solicitadas > cantidad_paginas_ocupadas){
        int paginas_a_aumentar = cantidad_paginas_solicitadas - cantidad_paginas_ocupadas;
        // 3. Segunda validación: ¿hay suficientes marcos libres?
        uint32_t* marcos_solicitados = buscar_marcos_libres(paginas_a_aumentar); // TODO: AGREGAR SINCRONIZACION EN LA FUNCIÓN
        if(!marcos_solicitados){
            op_code respuesta_cpu = OUT_OF_MEMORY;
            tiempo_espera_retardo(timer);
            ssize_t bytes_enviados = send(socket_cliente, &respuesta_cpu, sizeof(respuesta_cpu), 0);
            if (bytes_enviados == -1) {
                log_error(logger, "Error enviando dato OUT_OF_MEMORY a CPU");
                exit(EXIT_FAILURE);
            }
            return;
        }
        ampliar_proceso(pid, paginas_a_aumentar, marcos_solicitados);
    } else if (cantidad_paginas_solicitadas < cantidad_paginas_ocupadas){
        int paginas_a_reducir = cantidad_paginas_ocupadas - cantidad_paginas_solicitadas;
        reducir_proceso(pid, paginas_a_reducir);
    } // si cantidad_paginas_solicitadas == cantidad_paginas_ocupadas NO se hace nada :)
    op_code respuesta_cpu = CONTINUAR;
    tiempo_espera_retardo(timer);
    send(socket_cliente, &respuesta_cpu, sizeof(respuesta_cpu), 0);
    return;
}

// AMPLIACION 
void ampliar_proceso(uint32_t pid, int cantidad, uint32_t* marcos_solicitados){
    t_pcb* proceso = get_element_from_pid(pid);
    for(int i = 0; i < cantidad; i++)
        create_pagina(proceso->tabla_paginas, marcos_solicitados[i]); 
}

void reducir_proceso(uint32_t pid, int cantidad){
    t_pcb* proceso = get_element_from_pid(pid);
    liberar_paginas(proceso, cantidad);
}

/*          RETARDO CONFIG          */
void tiempo_espera_retardo(t_temporal* timer) {
    // cuento ms del timer y espero a que se complete el tiempo de retardo!
    int64_t tiempo_transcurrido = temporal_gettime(timer);

    if (tiempo_transcurrido < config.retardo_respuesta) {
        usleep((config.retardo_respuesta - tiempo_transcurrido) * 1000); // usleep espera en microsegundos
    }

    temporal_destroy(timer);
}
    
    
/*          ACCESO ESPACIO USUARIO            */
// uint32_t marco*tam_pagina + offset
// dir. marco*tam_pangina + offset
void* leer_memoria(uint32_t dir_fisica, int tam_lectura){
    void* dato = malloc(tam_lectura);
    // memcpy (donde guardo el dato (posicionado) / desde dónde saco el dato (posicionado) / el tamaño de lo que quiero sacar)
    memcpy(dato, memoria + dir_fisica, tam_lectura);
    return dato;
}


int escribir_memoria(uint32_t dir_fisica, void* dato, int tam_escritura){
    // memcpy (donde guardo el dato (posicionado) / desde dónde saco el dato (posicionado) / el tamaño de lo que quiero sacar)
    memcpy(memoria + dir_fisica, dato, tam_escritura);

    void* dato_escrito = leer_memoria(dir_fisica, tam_escritura);
    if(!dato_escrito)
        return DESALOJAR;
    else {
        log_debug(logger, "lo que acabamos de escribir ¿? %s .", (char*)dato_escrito);
        return CONTINUAR;
    }
}


