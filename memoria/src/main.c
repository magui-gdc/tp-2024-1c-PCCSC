#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

config_struct config;

int main(int argc, char* argv[]) {

    t_config* archivo_config = iniciar_config("memoria.config");
    cargar_config_struct_MEMORIA(archivo_config);
    logger = log_create("memoria.log", "Servidor Memoria", 1, LOG_LEVEL_DEBUG);
    
    decir_hola("Memoria");

    int socket_servidor = iniciar_servidor(config.puerto_escucha);
    log_info(logger, config.puerto_escucha);
    log_info(logger, "Server iniciado bip bop bop bip");

    int cliente;  
    while((cliente = esperar_cliente(socket_servidor)) != -1){
        int cod_op = recibir_operacion(cliente);
        switch (cod_op)
        {
        case CONEXION:
            recibir_conexion(cliente);
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
