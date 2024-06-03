#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

config_struct config;

int main(int argc, char* argv[]) {
    
    int conexion_kernel, conexion_memoria;
    //t_config* archivo_config = iniciar_config("entradasalida.config");
    t_log* logger = log_create("entradasalida.log", "Interfaz I/O", 1, LOG_LEVEL_DEBUG);

    decir_hola("una Interfaz de Entrada/Salida");

    /////////////////// CONEXIONES //////////////////////

    // establecer conexion con KERNEL
    conexion_kernel = crear_conexion(config.ip_kernel, config.puerto_kernel);
    enviar_conexion("Interfaz I/O", conexion_kernel);
    paquete(conexion_kernel);
    log_info(logger, "envie paquete a kernel");
    
    // establecer conexion con MEMORIA
    conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);
    enviar_conexion("Interfaz I/O", conexion_memoria);
    paquete(conexion_memoria);
    log_info(logger, "envie paquete a memoria");


    sem_t mutex_config;
    sem_init(&mutex_config, 0, 1); //TODO: destruir semaforo/s
    //TODO: llamar a inicializar_io(2), no sé si esto lo tiene que hacer el usuario o quién

    ////////////////////////////////////////////////////////


    ///////////////////////// GENERICA /////////////////////////



    ///////////////////////// STDIN    /////////////////////////



    //////////////////////// STDOUT    /////////////////////////



    ///////////////////////// DIALFS   /////////////////////////
 

    ///////////////////////////////////////////////////
    log_destroy(logger);
	config_destroy(archivo_config);
	liberar_conexion(conexion_kernel);
    liberar_conexion(conexion_memoria);

    return 0;
}

inicializar_io(char* nombre, t_config* archivo_config){
    t_io* interfaz = malloc(sizeof(t_io)); //TODO: free(interfaz)
    interfaz->nombre_id = nombre;
    interfaz->archivo = archivo_config;
    sem_wait(&mutex_config);
    IO_class clase = selector_carga_config(archivo_config);
    //TODO: desarrollar funciones según tipo
    switch (clase)
    {
    case GEN:
        char* tiempo = config.tiempo_unidad_trabajo;
        uint32_t iteraciones; //TODO: conseguir las iteraciones
        sleep(atoi(tiempo) * iteraciones);
        //TODO: enviar flag de llegada de IO al kernel
        break;
    case IN:

        break;
    case OUT:

        break;
    case FS:

        break;
    default:
        break;
    }
    sem_post(&mutex_config);
}

//////////////// CARGAR CONFIG SEGUN DISPOSITIVO IO  ////////////////

///////////////////////// GENÉRICA /////////////////////////

void cargar_config_struct_IO_gen(t_config* archivo_config){
    config.tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    config.tiempo_unidad_trabajo = config_get_string_value(archivo_config, "TIEMPO_UNIDAD_TRABAJO");
    config.ip_kernel = config_get_string_value(archivo_config, "IP_KERNEL");
    config.puerto_kernel = config_get_string_value(archivo_config, "PUERTO_KERNEL");
}

///////////////////////// STDIN    /////////////////////////

void cargar_config_struct_IO_in(t_config* archivo_config){
    config.tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    config.ip_kernel = config_get_string_value(archivo_config, "IP_KERNEL");
    config.puerto_kernel = config_get_string_value(archivo_config, "PUERTO_KERNEL");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
}

//////////////////////// STDOUT    /////////////////////////

void cargar_config_struct_IO_out(t_config* archivo_config){
    config.tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    config.tiempo_unidad_trabajo = config_get_string_value(archivo_config, "TIEMPO_UNIDAD_TRABAJO");
    config.ip_kernel = config_get_string_value(archivo_config, "IP_KERNEL");
    config.puerto_kernel = config_get_string_value(archivo_config, "PUERTO_KERNEL");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
}

///////////////////////// DIALFS   /////////////////////////

void cargar_config_struct_IO_fs(t_config* archivo_config){
    config.tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    config.tiempo_unidad_trabajo = config_get_string_value(archivo_config, "TIEMPO_UNIDAD_TRABAJO");
    config.ip_kernel = config_get_string_value(archivo_config, "IP_KERNEL");
    config.puerto_kernel = config_get_string_value(archivo_config, "PUERTO_KERNEL");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
    config.path_base_dialfs = config_get_string_value(archivo_config, "PATH_BASE_DIALFS");
    config.block_size = config_get_string_value(archivo_config, "BLOCK_SIZE");
    config.block_count = config_get_string_value(archivo_config, "BLOCK_COUNT");
    config.block_count = config_get_string_value(archivo_config, "RETRASO_COMPACTACION");

}

//////////////////////////////////////////// PRUEBA DE ENVIO DE PAQUETES /////////////////////////////////////////////////////

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

////////////// SELECTOR DE CARGA DE CONFIG SEGUN INTERFAZ IO ////////////////////////

void selector_carga_config (t_config* archivo_config){

    char* tipo_interfaz = config_get_string_value(archivo_config, "TIPO_INTERFAZ");
    IO_class clase;
    if (strcmp(tipo_interfaz, "GENERICA") == 0) {
        cargar_config_struct_IO_gen(archivo_config);
        clase = GEN;
    } else if (strcmp(tipo_interfaz, "STDIN") == 0) {
        cargar_config_struct_IO_in(archivo_config);
        clase = IN;
    } else if (strcmp(tipo_interfaz, "STDOUT") == 0) {
        cargar_config_struct_IO_out(archivo_config);
        clase = OUT;
    } else if (strcmp(tipo_interfaz, "DIALFS") == 0) {
        cargar_config_struct_IO_fs(archivo_config);
        clase = FS;
    } else {
        printf("interfaz no reconocida XD: %s\n", tipo_interfaz); // como se pone el log de q esta mal!!!?????
        clase = MISSING;
    }
    return clase;
}

////////////////////////// DIAL FS ////////////////////////////////////

