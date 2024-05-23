#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

config_struct config;
pthread_t thread_memoria;

int main(int argc, char *argv[])
{

    t_config *archivo_config = iniciar_config("memoria.config");
    cargar_config_struct_MEMORIA(archivo_config);
    logger = log_create("memoria.log", "Servidor Memoria", 1, LOG_LEVEL_DEBUG);

    decir_hola("Memoria");

    // iniciar servidor memoria

    int socket_servidor = iniciar_servidor(config.puerto_escucha);
    log_info(logger, config.puerto_escucha);
    log_info(logger, "Server MEMORIA iniciado");

    if (pthread_create(&thread_memoria, NULL, servidor_escucha, &socket_servidor) != 0)
    {
        log_error(logger, "No se ha podido crear el hilo SERVIDOR de MEMORIA");
        exit(EXIT_FAILURE);
    }

    pthread_join(thread_memoria, NULL);

    log_destroy(logger);
    config_destroy(archivo_config);

    return EXIT_SUCCESS;
}

void cargar_config_struct_MEMORIA(t_config *archivo_config)
{
    config.puerto_escucha = config_get_string_value(archivo_config, "PUERTO_ESCUCHA");
    config.tam_memoria = config_get_string_value(archivo_config, "TAM_MEMORIA");
    config.tam_pagina = config_get_string_value(archivo_config, "TAM_PAGINA");
    config.path_instrucciones = config_get_string_value(archivo_config, "PATH_INSTRUCCIONES");
    config.retardo_respuesta = config_get_string_value(archivo_config, "RETARDO_RESPUESTA");
}

void *atender_cliente(void *cliente)
{
    int cliente_recibido = *(int *)cliente;
    while (1)
    {
        int cod_op = recibir_operacion(cliente_recibido); // bloqueante
        switch (cod_op)
        {
        case INICIAR_PROCESO: // memoria recibe de kernel el proceso, recibe el path y lo chequea!!
            t_list *path_recibido = recibir_paquete(cliente_recibido);
            log_info(logger, "CREE EL PROCESO");
            break;
        case ELIMINAR_PROCESO: // memoria elimina el proceso, kernel le pasa el path o el pid
            break;
        case LEER_PROCESO:
            // TODO: PASARLO A UNA FC DENTRO DE BUFFER.C guarda BUFFER del paquete enviado
            t_sbuffer *buffer_lectura = malloc(sizeof(t_sbuffer));
            buffer_lectura->offset = 0; // ESTA LINEAAA ES MUY IMPORTANT!!!!!!!!!!!
            recv(cliente_recibido, &(buffer_lectura->size), sizeof(uint32_t), MSG_WAITALL);
            buffer_lectura->stream = malloc(buffer_lectura->size);
            recv(cliente_recibido, buffer_lectura->stream, buffer_lectura->size, MSG_WAITALL);

            // guarda datos del buffer (PID proceso + PC para buscar prox instruccion)
            uint32_t pid_proceso = buffer_read_uint32(buffer_lectura); // guardo PID del proceso del cual se quiere leer
            uint32_t pc_proceso = buffer_read_uint32(buffer_lectura);  // guardo PC para elegir la prox INSTRUCCION a ejecutar

            char *mensaje = (char *)malloc(128);
            sprintf(mensaje, "Recibi para leer del proceso %u, la prox INST desde PC %u", pid_proceso, pc_proceso);
            log_info(logger, "%s", mensaje);
            free(mensaje);

            // TODO: leer desde el proceso la próxima instruccion!!!!
            char nombreArchivo[256];
            snprintf(nombreArchivo, sizeof(nombreArchivo), "%d.txt", pid_proceso);

            int lineaActual = 0;

            char *instruccion = malloc(50);
            FILE *script = fopen(nombreArchivo, "rb");

            if (script == NULL)
                log_error(logger, "No se encontro ningun archivo con el nombre indicado...");
            else{
                while (fgets(instruccion, 50, script) != NULL){
                    lineaActual++;
                    if (lineaActual == pc_proceso){
                        char *mensaje = (char *)malloc(128);
                        sprintf(mensaje, "INSTRUCCION LEIDA EN LINEA %d: %s", pc_proceso, instruccion);
                        log_info(logger, "%s", mensaje);
                        free(mensaje);
                        break;
                    }
                }
                fclose(script);
                // TODO: QUE PASA SI LA ULTIMA INST DEL PROCESO NO FUE EXIT Y QUIERE SEGUIR EJECUTANDO
            }

            // TODO: send() a cpu la proxima linea con sus operandos (en caso de tener)
            t_sbuffer *buffer_instruccion = buffer_create(
                sizeof(instruccion)
            );

            buffer_add_string(buffer_instruccion, sizeof(instruccion), instruccion);
            cargar_paquete(cliente_recibido, INSTRUCCION, buffer_instruccion);

            free(instruccion);
            buffer_destroy(buffer_lectura);
            break;
        case CONEXION:
            recibir_conexion(cliente_recibido);
            break;
        case PAQUETE:
            t_list *lista = recibir_paquete(cliente_recibido);
            log_info(logger, "Me llegaron los siguientes valores:\n");
            list_iterate(lista, (void *)iterator); // esto es un mapeo
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