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
        log_error(logger, "No se ha podido crear el hilo para la conexion con interfaces I/O");
        exit(EXIT_FAILURE);
    } 



    int cliente;  
    while((cliente = esperar_cliente(socket_servidor)) != -1){
        int cod_op = recibir_operacion(cliente);
        switch (cod_op)
        {
        case CONEXION:
            recibir_conexion(cliente);
            break;
        case PAQUETE:
        	log_info(logger, "ATENCION! ME ESTA POR LLEGAR UN PAQUETE JIJIJI:\n");
			t_list* lista = recibir_paquete(cliente);
			log_info(logger, "Me llegaron los siguientes valores:\n");
			list_iterate(lista, (void*) iterator); //esto es un mapeo
			break;
        case -1:
            log_error(logger, "cliente desconectado");
            break;
        default:
            log_warning(logger, "Operacion desconocida.");
            break;
        }
    }

    return EXIT_SUCCESS;
}

void cargar_config_struct_MEMORIA(t_config* archivo_config){
    config.puerto_escucha = config_get_string_value(archivo_config, "PUERTO_ESCUCHA");
    config.tam_memoria = config_get_string_value(archivo_config, "TAM_MEMORIA");
    config.tam_pagina = config_get_string_value(archivo_config, "TAM_PAGINA");
    config.path_instrucciones = config_get_string_value(archivo_config, "PATH_INSTRUCCIONES");
    config.retardo_respuesta = config_get_string_value(archivo_config, "RETARDO_RESPUESTA");
}
