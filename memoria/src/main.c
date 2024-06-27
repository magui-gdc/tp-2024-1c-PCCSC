#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "paginacion.h" 
#include "main.h"
#include <commons/temporal.h>

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
    bitarray_destroy(bitmap_marcos_libres);

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
        case PEDIR_FRAME:
           /* t_sbuffer* buffer_pagina = cargar_buffer(cliente_recibido);
            int pagina_enviada = buffer_read_int(buffer_pagina);
            int frame = buscar_frame(pagina_enviada);
            if (frame == -1){
                
            } */
        break;
        case INICIAR_PROCESO: // memoria recibe de kernel el proceso, recibe el path y lo chequea!!
            t_sbuffer *buffer_path = cargar_buffer(cliente_recibido);
            uint32_t pid_iniciar = buffer_read_uint32(buffer_path);
            uint32_t longitud_path;
            char* path  = buffer_read_string(buffer_path, &longitud_path);
            path[strcspn(path, "\n")] = '\0'; // CORREGIR: DEBE SER UN PROBLEMA DESDE EL ENVÍO DEL BUFFER!

            crear_proceso(pid_iniciar, path, longitud_path); 
            op_code respuesta_kernel = PROCESO_CREADO; // si todo fue ok enviar PROCESO_CREADO para que kernel pueda continuar con su planificacion
            ssize_t bytes_enviados = send(cliente_recibido, &respuesta_kernel, sizeof(respuesta_kernel), 0);
            if (bytes_enviados == -1) {
                log_error(logger, "Error enviando el dato");
                exit(EXIT_FAILURE);
            }
        break;
        case MOV_IN:
            // TODO: leer buffer
            // y hacer cosas
            // si fue todo bien: mensaje CONTINUAR
            /*
            en caso de continuar(que no necesitas mandar nada mas qye le mensaje)
            op_code respuesta_cpu = CONTINUAR;
            ssize_t bytes_enviados = send(cliente_recibido, &respuesta_cpu, sizeof(respuesta_cpu), 0);
            if (bytes_enviados == -1) {
                perror("Error enviando el dato");
                exit(EXIT_FAILURE);
            }
            */
            // mandas este codigo OUT_OF_MEMORY
            // LIBERAR buffer, memoria utilizada dinamicamente..
        break;
        case MOV_OUT:
            
        break;
        case ELIMINAR_PROCESO: // memoria elimina el proceso, kernel le pasa el path o el pid
            t_sbuffer* buffer_eliminar = cargar_buffer(cliente_recibido);
            uint32_t pid_eliminar = buffer_read_uint32(buffer_eliminar);
            log_debug(logger, "se mando a eliminar/liberar proceso %u", pid_eliminar);
            eliminar_proceso(pid_eliminar); 
        break;
        case LEER_PROCESO:
            // 1. comienza a contar el timer para calcular retardo en rta a CPU
            t_temporal* timer = temporal_create();

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

                    // cuento ms del timer y espero a que se complete el tiempo de retardo!
                    int64_t tiempo_transcurrido = temporal_gettime(timer);

                    if (tiempo_transcurrido < config.retardo_respuesta) {
                        usleep((config.retardo_respuesta - tiempo_transcurrido) * 1000); // usleep espera en microsegundos
                    }
                    cargar_paquete(cliente_recibido, INSTRUCCION, buffer_instruccion);

                    buffer_destroy(buffer_lectura);
                    temporal_destroy(timer);
                } // else TODO: llega al final y CPU todavía no desalojó el proceso por EXIT (esto pasaría sólo si el proceso no termina con EXIT)
                free(instruccion);
            }
            free(path_instrucciones_proceso);
        break;
        case -1:
            log_error(logger, "Cliente desconectado.");
            close(cliente_recibido); // cierro el socket accept del cliente
            free(cliente);           // libero el malloc reservado para el cliente
            pthread_exit(NULL);      // solo sale del hilo actual => deja de ejecutar la función atender_cliente que lo llamó
        default:
            log_warning(logger, "Operacion desconocida.");
            break;
        }
    }
}


////// funciones memoria
// crear_proceso y eliminar_proceso quedaron definidos en paginacion.c

// RESIZE

void resize_proceso(uint32_t pid, int new_size){
    // int size_resultante = size_actual - new_size 
    // if size_resultante > 0 -> es para reducir... else if size_resultante < 0 -> es para ampliar el proceso.. else nada.
}

// AMPLIACION 
void ampliar_proceso(uint32_t pid, int new_size){
    
    /*
    int cant_paginas_requeridas = new_size/config.tam_memoria;
    if(!suficiente_memoria(new_size) || ! suficientes_marcos(cant_paginas_requeridas)){
        log_error(logger, "No hay suficiente espacio para este proceso");
        exit(OUT_OF_MEMORY);
    }*/
}

