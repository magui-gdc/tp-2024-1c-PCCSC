#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

config_struct config;

int main(int argc, char* argv[]) {
    
    int conexion_kernel, conexion_memoria;
    t_config* archivo_config = iniciar_config("entradasalida.config");
    t_log* logger = log_create("entradasalida.log", "Interfaz I/O", 1, LOG_LEVEL_DEBUG);

    cargar_config_struct_IO(archivo_config);

    decir_hola("una Interfaz de Entrada/Salida");

    // establecer conexion con KERNEL
    conexion_kernel = crear_conexion(config.ip_kernel, config.puerto_kernel);
    enviar_conexion("Interfaz I/O", conexion_kernel);
    paquete(conexion_kernel);
    log_info(logger, "envie paquete a kernel");
    
    
    // establecer conexion con MEMORIA
    conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);
    enviar_conexion("Interfaz I/O", conexion_memoria);
    //paquete(conexion_memoria);
    //log_info(logger, "envie paquete a memoria");

    log_destroy(logger);
	config_destroy(archivo_config);
	liberar_conexion(conexion_kernel);
    liberar_conexion(conexion_memoria);

    return 0;
}


void cargar_config_struct_IO(t_config* archivo_config){
    config.tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    config.tiempo_unidad_trabajo = config_get_string_value(archivo_config, "TIEMPO_UNIDAD_TRABAJO");
    config.ip_kernel = config_get_string_value(archivo_config, "IP_KERNEL");
    config.puerto_kernel = config_get_string_value(archivo_config, "PUERTO_KERNEL");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
    config.path_base_dialfs = config_get_string_value(archivo_config, "PATH_BASE_DIALFS");
    config.block_size = config_get_string_value(archivo_config, "BLOCK_SIZE");
    config.block_count = config_get_string_value(archivo_config, "BLOCK_COUNT");
}

void paquete(int conexion) {
	char* leido = malloc(sizeof(char));
	t_paquete* paquete;
    int tamanio;

	paquete = crear_paquete();

	while (1){
		leido = readline("> ");

		if(strlen(leido)==0){
			free(leido);
			break;
		}

		tamanio = strlen(leido) + 1;
		agregar_a_paquete(paquete, leido, tamanio);
		// libero lineas
		free(leido);
	}

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}