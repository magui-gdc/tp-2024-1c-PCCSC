#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

config_struct config;
pthread_t thread_memoria;

int main(int argc, char* argv[]) {

    t_config* archivo_config = iniciar_config("memoria.config");
    cargar_config_struct_MEMORIA(archivo_config);
    logger = log_create("memoria.log", "Servidor Memoria", 1, LOG_LEVEL_DEBUG);
    
    decir_hola("Memoria");

    //iniciar servidor memoria

    int socket_servidor = iniciar_servidor(config.puerto_escucha);
    log_info(logger, config.puerto_escucha);
    log_info(logger, "Server MEMORIA iniciado");

    if(pthread_create(&thread_memoria, NULL, servidor_escucha, &socket_servidor) != 0) {
        log_error(logger, "No se ha podido crear el hilo SERVIDOR de MEMORIA");
        exit(EXIT_FAILURE);
    } 

    pthread_join(thread_memoria, NULL);

    log_destroy(logger);
    config_destroy(archivo_config);

    return EXIT_SUCCESS;
}

void cargar_config_struct_MEMORIA(t_config* archivo_config){
    config.puerto_escucha = config_get_string_value(archivo_config, "PUERTO_ESCUCHA");
    config.tam_memoria = config_get_string_value(archivo_config, "TAM_MEMORIA");
    config.tam_pagina = config_get_string_value(archivo_config, "TAM_PAGINA");
    config.path_instrucciones = config_get_string_value(archivo_config, "PATH_INSTRUCCIONES");
    config.retardo_respuesta = config_get_string_value(archivo_config, "RETARDO_RESPUESTA");
}

void* atender_cliente(void* cliente){
	int cliente_recibido = *(int*) cliente;
	while(1){
		int cod_op = recibir_operacion(cliente_recibido); // bloqueante
		switch (cod_op)
		{
        case INICIAR_PROCESO: //memoria recibe de kernel el proceso, recibe el path y lo chequea!!
            t_list* path_recibido = recibir_paquete(cliente_recibido);
            log_info(logger, "CREE EL PROCESO")
            break;
        case ELIMINAR_PROCESO: //memoria elimina el proceso, kernel le pasa el path o el pid
            break;
		case CONEXION:
			recibir_conexion(cliente_recibido);
			break;
		case PAQUETE:
			t_list* lista = recibir_paquete(cliente_recibido);
			log_info(logger, "Me llegaron los siguientes valores:\n");
			list_iterate(lista, (void*) iterator); //esto es un mapeo
			break;
		case -1:
			log_error(logger, "Cliente desconectado.");
			close(cliente_recibido); // cierro el socket accept del cliente
			free(cliente); // libero el malloc reservado para el cliente
			pthread_exit(NULL); // solo sale del hilo actual => deja de ejecutar la función atender_cliente que lo llamó
		default:
			log_warning(logger, "Operacion desconocida.");
			break;
		}
	}
}