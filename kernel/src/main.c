#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

config_struct config;

int main(int argc, char* argv[]) {

    int conexion_cpu_dispatch, conexion_cpu_dispatch2, conexion_memoria, conexion_cpu_interrupt;
    t_config* archivo_config = iniciar_config("kernel.config");
    cargar_config_struct_KERNEL(archivo_config);
    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);
    
    pthread_t thread_kernel;
    
    /*
    Ademas de las operaciones de la consola interactiva, el Kernel sera el encargado de gestionar las peticiones contra la Memoria y las interfaces de I/O 
    (conectadas dinamicamente), por lo que debera implementarse siguiendo una estrategia multihilo que permita la concurrencia de varias solicitudes desde o hacia diferentes madulos.
    Sumado a esto, el Kernel se encargara de planificar la ejecucion de los procesos del sistema en el modulo CPU a traves de dos conexiones con el mismo:  una de dispatch y otra de interrupt.
    */

    decir_hola("Kernel");

    // CONEXION CLIENTE - SERVIDORES
    // a cpu
    conexion_cpu_dispatch = crear_conexion(config.ip_cpu, config.puerto_cpu_dispatch);
    log_info(logger, "se conecta a CPU puerto DISPATCH 1");
    enviar_conexion("Kernel 1 a DISPATCH", conexion_cpu_dispatch);

    conexion_cpu_dispatch2 = crear_conexion(config.ip_cpu, config.puerto_cpu_dispatch);
    log_info(logger, "se conecta a CPU puerto DISPATCH 2");
    enviar_conexion("Kernel 2 a DISPATCH", conexion_cpu_dispatch2);

    conexion_cpu_interrupt = crear_conexion(config.ip_cpu, config.puerto_cpu_interrupt);
    log_info(logger, "se conecta a CPU puerto INTERRUPT");
    enviar_conexion("Kernel a INTERRUPT", conexion_cpu_interrupt);
    
	liberar_conexion(conexion_cpu_dispatch); 
    liberar_conexion(conexion_cpu_dispatch2); 
    liberar_conexion(conexion_cpu_interrupt);
    
    // a memoria
    log_info(logger, "se conecta a MEMORIA puerto INTERRUPT");
    conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);
    enviar_conexion("Kernel", conexion_memoria);
    

    // CONEXION SERVIDOR - CLIENTES
    int socket_servidor = iniciar_servidor(config.puerto_escucha);
    log_info(logger, config.puerto_escucha);
    log_info(logger, "Server KERNEL iniciado");
    
    // un hilo por consola: desde donde se crean procesos, matan procesos, se detiene planificacion ...
    // un hilo por solicitud a memoria

    // un hilo por espera de I/O
    pthread_create(&thread_kernel, NULL, servidor_escucha, &socket_servidor); 
    
    pthread_join(thread_kernel, NULL);
    
    log_destroy(logger);
	config_destroy(archivo_config);
    liberar_conexion(conexion_memoria);
    
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
