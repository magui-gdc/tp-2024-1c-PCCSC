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

     //----- RECIBO ARCHIVO CONFIG E INICIALIZO LA IO
    t_config* arch_config = config_create(obtener_path());
    char* nombre = obtener_path(); //TODO: esto en realidad lo nombra como el path, hay que buscar una forma que dado el path tengamos el nombre del file
    t_io* interfaz_io = inicializar_io(nombre, arch_config); //TODO: free(interfaz_io)
    free(nombre);
    free(arch_config);

    // EN ESTE PUNTO LA IO CUENTA CON UN CONFIG CARGADO Y UNA INTERFAZ QUE CONTIENE TANTO EL ARCHIVO CONFIG, COMO SU NOMBRE Y LA CLASE

    /////////////////// CONEXIONES //////////////////////

    // establecer conexion con KERNEL
    conexion_kernel = crear_conexion(config.ip_kernel, config.puerto_kernel);
    //enviar_conexion("Interfaz I/O", conexion_kernel);

   
    //----- IDENTIFICO LA IO CON EL KERNEL
    //TODO: el kernel deber verificar que la IO no exista aun para no iniciarla dos veces y meterla en una lista de IOs de su tipo
    
    t_sbuffer* buffer_conexion = buffer_create(
        (uint32_t)strlen(interfaz_io->nombre_id) + sizeof(uint32_t) // tamanio del string
        + (uint32_t)strlen(config.tipo_interfaz) + sizeof(uint32_t) // + sumar todos los sizeof de los valores que le vas a pasar (tipo_interfaz, path , ...)
    );

    buffer_add_string(buffer_conexion, (uint32_t)strlen(interfaz_io->nombre_id), interfaz_io->nombre_id);
    buffer_add_string(buffer_conexion, (uint32_t)strlen(config.tipo_interfaz), config.tipo_interfaz)
    cargar_paquete(conexion_kernel, CONEXION, buffer_conexion);

    paquete(conexion_kernel);
    log_info(logger, "envie paquete a kernel");
    
    // establecer conexion con MEMORIA
    conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);
    enviar_conexion("Interfaz I/O", conexion_memoria);
    paquete(conexion_memoria);
    log_info(logger, "envie paquete a memoria");


    sem_t mutex_config;
    sem_init(&mutex_config, 0, 1); //TODO: destruir semaforo/s

    ////////////////////////////////////////////////////////
    //TODO: desarrollar funciones según tipo
    
    //TODO: esperar que reciba comunicacion del kernel y verificar que el codigo de abajo este bien xd

    //TODO: free() punteros
    t_sbuffer* buffer_operacion = cargar_buffer(conexion_kernel);
    int op = recibir_operacion(buffer_operacion);
    uint32_t length;
    char* leido = buffer_read_string(buffer_operacion, &length); 
    //leido[strcspn(leido, "\n")] = '\0'; ??
    char **tokens = string_split(leido, " ");


    switch (interfaz_io->clase)
    {
    case GEN:
        //TODO: conseguir las iteraciones}
        if(op == IO_GEN_SLEEP){
            uint32_t iteraciones = (uin32_t)atoi(tokens[2]);
            io_gen_sleep(iteraciones);
        }
        //TODO: enviar flag de llegada de IO al kernel
        break;
    case IN:
        if(op == IO_STDIN_READ){

        }
        break;
    case OUT:
        if(op == IO_STDOUT_WRITE){
            
        }
        break;
    case FS:
        if(op == IO_FS_CREATE){
            
        }
        if(op == IO_FS_DELETE){
            
        }
        if(op == IO_FS_TRUNCATE){
            
        }
        if(op == IO_FS_WRITE){
            
        }
        if(op == IO_FS_READ){
            
        }
        break;
    default:
        break;
    }


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



//////////////////////////////////////////// PRUEBA DE ENVIO DE PAQUETES ///////////////////////////(XD)//////////////////////////

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

////////////// CONFIGURACIÓN DEL CONFIG Y DERIVADOS ////////////////////////

inicializar_io(char* nombre, t_config* archivo_config){
    t_io* interfaz = malloc(sizeof(t_io)); //TODO: free(interfaz)
    interfaz->nombre_id = nombre;
    interfaz->archivo = archivo_config;
    interfaz->clase = selector_carga_config(archivo_config);
    
    return interfaz;
}

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

char* obtener_path(){
    char* path = malloc(sizeof(char) * 100);
    printf("introduzca el path de la IO");
    scanf("%s", path);
    if(strlen(path) == 0){
        //TODO: retornar error por falta de nombre;
    }
    return path;
}


//-----------------CARGAR CONFIG SEGUN DISPOSITIVO IO-----------------//

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

//-----------------FUNCIONES DE IO-----------------//

    ///////////////////////// GENERICA /////////////////////////

void io_gen_sleep(uint32_t iteraciones){
    char* tiempo = config.tiempo_unidad_trabajo;
    sleep(atoi(tiempo) * iteraciones);
}

    ///////////////////////// STDIN    /////////////////////////

void io_stdin_read(char* direc, uint32_t size){

}

    //////////////////////// STDOUT    /////////////////////////

void io_stdout_write(char* direc, uint32_t size){

}

    ///////////////////////// DIALFS   /////////////////////////

void io_fs_create(char* arch){

}

void io_fs_delete(char* arch){

}

void io_fs_truncate(char* arch, uint32_t size){
    
}

void io_fs_write(char* arch, char* direc, uint32_t size, uint32_t pointer){

}

void io_fs_read(char* arch, char* direc, uint32_t size, uint32_t pointer){

}