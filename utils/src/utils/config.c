#include "config.h"

// CONFIGURACION
t_config* iniciar_config(char* nombre_archivo_configuracion){
	t_config* nuevo_config = config_create(nombre_archivo_configuracion);
	if(nuevo_config == NULL){
		printf("No se pudo obtener el archivo de configuracion %s\n", nombre_archivo_configuracion);
		perror("Error");
		exit(EXIT_FAILURE);
	}

	return nuevo_config;
}
