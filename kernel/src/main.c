#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

int main(int argc, char* argv[]) {

    t_config* archivo_config = iniciar_config("kernel.config");
    cargar_config_struct_kernel(archivo_config);
    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);
    
    decir_hola("Kernel");

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

void cargar_config_struct_KERNEL(t_config* archivo_config){
    config.puerto_escucha = config_get_string_value(archivo_config, "PUERTO_ESCUCHA");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
    config.ip_cpu = config_get_string_value(archivo_config, "IP_CPU");
    config.puerto_cpu_dispatch = config_get_string_value(archivo_config, "PUERTO_CPU_DISPATCH");
    config.puerto_cpu_interrupt = config_get_string_value(archivo_config, "PUERTO_CPU_INTERRUPT");
    config.algoritmo_planificacion = config_get_string_value(archivo_config, "ALGORITMO_PLANIFICACION");
    config.recursos = config_get_array_value(archivo_config, "RECURSOS" );
    config.instancias = config_get_array_value(archivo_config, "INSTANCIAS");
    config.grado_multiprogramacion = config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION");
}
