#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

int main(int argc, char* argv[]) {
    
    int conexion_kernel, conexion_memoria;
    t_config* config = iniciar_config_cliente("entradasalida.config");
    t_log* logger = log_create("entradasalida.log", "Interfaz I/O", 1, LOG_LEVEL_DEBUG);

    decir_hola("una Interfaz de Entrada/Salida");

    // establecer conexion con KERNEL
    char* ip_kernel = config_get_string_value(config, "IP_KERNEL");
    char* puerto_kernel = config_get_string_value(config, "PUERTO_KERNEL");
    conexion_kernel = crear_conexion(ip_kernel, puerto_kernel);
    enviar_conexion("Interfaz I/O", conexion_kernel);
    
    
    // establecer conexion con MEMORIA
    char* ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    char* puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_conexion("Interfaz I/O", conexion_memoria);
    

    log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion_kernel);
    liberar_conexion(conexion_memoria);

    return 0;
}
