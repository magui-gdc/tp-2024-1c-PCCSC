#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

config_struct config;

int main(int argc, char* argv[]) {

    int conexion_cpu_dispatch, conexion_memoria, conexion_cpu_interrupt;
    pthread_t thread_kernel_servidor, thread_kernel_consola, thread_planificador_corto_plazo, thread_planificador_largo_plazo;

    // ------------ ARCHIVOS CONFIGURACION + LOGGER ------------
    t_config* archivo_config = iniciar_config("kernel.config");
    cargar_config_struct_KERNEL(archivo_config);
    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);
    
    decir_hola("Kernel");

    // ------------ CONEXION CLIENTE - SERVIDORES ------------
    // conexion puertos cpu
    conexion_cpu_dispatch = crear_conexion(config.ip_cpu, config.puerto_cpu_dispatch);
    log_info(logger, "se conecta a CPU puerto DISPATCH");
    enviar_conexion("Kernel a DISPATCH", conexion_cpu_dispatch);

    conexion_cpu_interrupt = crear_conexion(config.ip_cpu, config.puerto_cpu_interrupt);
    log_info(logger, "se conecta a CPU puerto INTERRUPT");
    enviar_conexion("Kernel a INTERRUPT", conexion_cpu_interrupt);
    
    // conexion memoria
    log_info(logger, "se conecta a MEMORIA puerto INTERRUPT");
    conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);
    enviar_conexion("Kernel", conexion_memoria);
    

    // ------------ CONEXION SERVIDOR - CLIENTES ------------
    int socket_servidor = iniciar_servidor(config.puerto_escucha);
    log_info(logger, config.puerto_escucha);
    log_info(logger, "Server KERNEL iniciado");
    

    // ------------ HILOS ------------
    // hilo con MULTIPLEXACION a Interfaces I/O
    pthread_create(&thread_kernel_servidor, NULL, servidor_escucha, &socket_servidor); 
    // hilo para recibir mensajes por consola
    pthread_create(&thread_kernel_consola, NULL, consola_kernel, NULL);
    // hilo para planificacion a corto plazo (READY A EXEC)
    pthread_create(&thread_planificador_corto_plazo, NULL, planificar_corto_plazo, NULL);
    // hilo para planificacion a largo plazo (NEW A READY)
    pthread_create(&thread_planificador_corto_plazo, NULL, planificar_largo_plazo, NULL);
   
    pthread_join(thread_kernel_servidor, NULL);
    pthread_join(thread_kernel_consola, NULL);
    pthread_join(thread_planificador_corto_plazo, NULL);
    pthread_join(thread_planificador_largo_plazo, NULL);
    
    log_destroy(logger);
	config_destroy(archivo_config);
    liberar_conexion(conexion_memoria);
	liberar_conexion(conexion_cpu_dispatch); 
    liberar_conexion(conexion_cpu_interrupt);
    
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

void* consola_kernel(void*arg){
    while(1){
        // IMPLEMENTAR CONSOLA DINAMICA CON READLINE
        /*
        switch (leido_consola)
        {
        case "EJECUTAR_SCRIPT [PATH]":
            // acciones
            break;

        // ...
            
        default:
            break; // si no se ingreso ninguna de las opciones sigue en ejecucion esperando
        }
        */
    }
}

void* planificar_corto_plazo(void* arg){
    while(1){
        // PLANIFICACION corto plazo
    }
}

void* planificar_largo_plazo(void* arg){
    while(1){
        // PLANIFICACION Largo plazo
    }
}
