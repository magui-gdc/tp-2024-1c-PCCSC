#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

config_struct config;

int main(int argc, char* argv[]) {
    
    int conexion_kernel, conexion_memoria;
    logger = log_create("entradasalida.log", "Interfaz I/O", 1, LOG_LEVEL_DEBUG);

    decir_hola("una Interfaz de Entrada/Salida");

     //----- RECIBO ARCHIVO CONFIG E INICIALIZO LA IO
    t_config* archivo_config = config_create(obtener_path());
    char* nombre = obtener_path(); //TODO: esto en realidad lo nombra como el path, hay que buscar una forma que dado el path tengamos el nombre del file
    t_io* interfaz_io = inicializar_io(nombre, archivo_config); //TODO: free(interfaz_io)

    // EN ESTE PUNTO LA IO CUENTA CON UN CONFIG CARGADO Y UNA INTERFAZ QUE CONTIENE TANTO EL ARCHIVO CONFIG, COMO SU NOMBRE Y LA CLASE

    /////////////////// CONEXIONES //////////////////////

    // establecer conexion con KERNEL
    conexion_kernel = crear_conexion(config.ip_kernel, config.puerto_kernel);
   
    //----- IDENTIFICO LA IO CON EL KERNEL
    // TODO: el kernel deber verificar que la IO no exista aun para no iniciarla dos veces y meterla en una lista de IOs de su tipo
    
    t_sbuffer* buffer_conexion = buffer_create(
        (uint32_t)strlen(interfaz_io->nombre_id) + sizeof(uint32_t) // tamanio del string
        + (uint32_t)strlen(config.tipo_interfaz) + sizeof(uint32_t) // + sumar todos los sizeof de los valores que le vas a pasar (tipo_interfaz, path , ...)
    );

    buffer_add_string(buffer_conexion, (uint32_t)strlen(interfaz_io->nombre_id), interfaz_io->nombre_id);
    buffer_add_string(buffer_conexion, (uint32_t)strlen(config.tipo_interfaz), config.tipo_interfaz);
    cargar_paquete(conexion_kernel, CONEXION, buffer_conexion);
    log_info(logger, "Envie conexion a KERNEL");
    
    // establecer conexion con MEMORIA

    if(interfaz_io->clase != GEN){
        conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);

        log_info(logger, "Envie conexion a MEMORIA");
    }


    sem_t mutex_config;
    sem_init(&mutex_config, 0, 1); //TODO: destruir semaforo/s

    ////////////////////////////////////////////////////////
    //TODO: desarrollar funciones según tipo
    
    //TODO: esperar que reciba comunicacion del kernel y verificar que el codigo de abajo este bien xd

    //TODO: free() punteros

    // un ciclo para esperar constantemente indicaciones del lado KERNEL
    while(1){

        // 1. siempre ejecuta recibir_operación antes de cargar el buffer ya que recibir_operación hace un RECV bloqueante y lee la operación que es LO PRIMERO que se manda en un paquete, luego el buffer en sí 
        int op = recibir_operacion(conexion_kernel); // DE MEMORIA SIEMPRE ENVIA A DEMANDA A PARTIR DE LAS PETICIONES DE KERNEL POR LO QUE NO HACE FALTA CICLO 
        
        // 2. Se inicializa el buffer. Se lee lo que haya en el buffer desde cada operación ya que conociendo la operacion sabes qué tenes que recibir/leer
        t_sbuffer* buffer_operacion = cargar_buffer(conexion_kernel); 

        // 3. Se procesa la operación
        switch (interfaz_io->clase){
        case GEN:
            if(op == IO_GEN_SLEEP){
                // A. ACA RECIBE EL RESTO DEL CONTENIDO DEL BUFFER PORQUE SABE LO QUE DEBERÍA RECIBIR 
                // EN ESE CASO RECIBIRÍA UN NÚMERO PARA HACER LAS ITERACIONES
                uint32_t iteraciones = buffer_read_uint32(buffer_operacion);

                // B. Ejecutar instrucción!
                io_gen_sleep(iteraciones);

                // C. Responder a KERNEL
                op_code respuesta_kernel = IO_LIBERAR; // si todo fue ok SIEMPRE enviar IO_LIBERAR
                ssize_t bytes_enviados = send(conexion_kernel, &respuesta_kernel, sizeof(respuesta_kernel), 0);
                if (bytes_enviados == -1) {
                    log_error(logger, "Error enviando el dato");
                    exit(EXIT_FAILURE);
                }
            }
            break;
        case IN:
            if(op == IO_STDIN_READ){
                uint32_t direc = buffer_read_uint32(buffer_operacion);
                uint32_t size = buffer_read_uint32(buffer_operacion);

                io_stdin_read(direc, size, conexion_memoria);

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
                // todo: se le habla a memoria
                
            }
            if(op == IO_FS_READ){
                
            }
            break;
        default:
            break;
        }


        // Liberar memoria
        buffer_destroy(buffer_operacion); // liberar SIEMPRE el buffer después de usarlo
    }


    ///////////////////////// GENERICA /////////////////////////



    ///////////////////////// STDIN    /////////////////////////



    //////////////////////// STDOUT    /////////////////////////



    ///////////////////////// DIALFS   /////////////////////////
 

    ///////////////////////////////////////////////////
    log_destroy(logger);
    free(nombre);
	config_destroy(archivo_config);
	liberar_conexion(conexion_kernel);
    liberar_conexion(conexion_memoria);

    return 0;
}



//////////////////////////////////////////// PRUEBA DE ENVIO DE PAQUETES ///////////////////////////(XD)//////////////////////////
/*
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
*/

////////////// CONFIGURACIÓN DEL CONFIG Y DERIVADOS ////////////////////////

t_io* inicializar_io(char* nombre, t_config* archivo_config){
    t_io* interfaz = malloc(sizeof(t_io)); //TODO: free(interfaz)
    interfaz->nombre_id = nombre;
    interfaz->archivo = archivo_config;
    interfaz->clase = selector_carga_config(archivo_config);
    
    return interfaz;
}

IO_class selector_carga_config (t_config* archivo_config){

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
        log_error(logger, "interfaz no reconocida XD: %s", tipo_interfaz);
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
    usleep(atoi(tiempo) * iteraciones * 1000); // EL TIEMPO UNIDAD TRABAJO ESTÁ EXPRESADO EN MS => usar usleep que trabaja con microsegundos! 
}

    ///////////////////////// STDIN    /////////////////////////

void io_stdin_read(uint32_t direc, uint32_t size, int socket){

    // CARGO VALOR

    uint32_t asig_esp = sizeof(char) * size;
    char* input = malloc(asig_esp);
    scanf("%s", input); //TODO: verificar el overflow error
    
    // VERIFICO OVFLOW

    if(strlen(input) > size){
        log_error(logger, "El input produjo un overflow");
        exit(EXIT_FAILURE);
    }

    // CREO Y MANDO EL BUFFER

    t_sbuffer* buffer_memoria = buffer_create(
        (uint32_t)strlen(input) + sizeof(uint32_t) // string a ingresar
        + sizeof(uint32_t) // direccion de memoria
    );

    buffer_add_string(buffer_memoria, (uint32_t)strlen(input), input);
    buffer_add_uint32(direc);
    //TODO: aca no va IO_STDIN_READ, sino la instruccion que corresponda al cargado de datos en memoria (que puede ser esta misma si quieren)
    cargar_paquete(socket, IO_STDIN_READ, buffer_memoria);

    // ESPERO RESPUESTA DE MEMORIA

    //TODO: verificar el "ok" de memoria

    int ok = 0;
    while (ok == 0){ // este while tal vez esta de mas
        int op_code = recibir_operacion(socket);
        if(op_code == IO_MEMORY_DONE){
            ok = buffer_read_int(buffer_memoria) //?
            if(ok == -1){ //?
                log_error(logger, "Error al cargar valor en memoria");
                exit(EXIT_FAILURE);
            }
        }
    }
    
    buffer_destroy(buffer_memoria);

}

    //////////////////////// STDOUT    /////////////////////////

void io_stdout_write(uint32_t direc, uint32_t size, int socket){

}

    ///////////////////////// DIALFS   /////////////////////////

void io_fs_create(char* arch, int socket){

}

void io_fs_delete(char* arch, int socket){

}

void io_fs_truncate(char* arch, uint32_t size, int socket){
    
}

void io_fs_write(char* arch, uint32_t direc, uint32_t size, uint32_t pointer, int socket){

}

void io_fs_read(char* arch, uint32_t direc, uint32_t size, uint32_t pointer, int socket){

}


// definirla para que no lance error la compilación por UTILSERVER.C
void* atender_cliente(void* cliente) {
    return NULL;
}