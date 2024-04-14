#include "utilsServer.h"

t_log* logger;

// CONFIGURACION
t_config* iniciar_config(char* nombre_archivo_configuracion, char** puerto)
{
	t_config* nuevo_config = config_create(nombre_archivo_configuracion);
	if(nuevo_config == NULL){
		printf("No se pudo obtener el archivo de configuracion %s\n", nombre_archivo_configuracion);
		perror("Error");
		exit(EXIT_FAILURE);
	}

	// toma puerto del servidor actual
	if(config_has_property(nuevo_config, "PUERTO_ESCUCHA"))
		*puerto = config_get_string_value(nuevo_config, "PUERTO_ESCUCHA");
	return nuevo_config;
}

// CONEXION CLIENTE - SERVIDOR
//recibe PUERTO, ya que no todos los servidores pueden estar en el mismo puerto
int iniciar_servidor(char* PUERTO){

	int opt = 1;

  	int socket_servidor;
	int err;
	struct addrinfo hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(NULL, PUERTO, &hints, &servinfo);

	socket_servidor = socket(servinfo->ai_family,
                        	servinfo->ai_socktype,
                   		    servinfo->ai_protocol);

    if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) 
    {
        log_error(logger,"NO SE PUDO LIBERAR EL PUERTO >:(");
	    exit(EXIT_FAILURE);
    }

	err = bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

	err = listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);

	log_trace(logger, "Listo para escuchar a mi cliente");

	return socket_servidor;
}

int esperar_cliente(int socket_servidor){
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	log_info(logger, "Se conecto un cliente!");

	return socket_cliente;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente){
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

void recibir_conexion(int socket_cliente){
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	log_info(logger, "Se conecto %s", buffer);
	free(buffer);
}