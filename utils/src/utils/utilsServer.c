#include "utilsServer.h"

t_log* logger;

// CONEXION CLIENTE - SERVIDOR
//recibe PUERTO, ya que no todos los servidores pueden estar en el mismo puerto

int iniciar_servidor(char* PUERTO){

	int opt = 1;

  	int socket_servidor;
	int err;
	struct addrinfo hints, *servinfo;

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
	int socket_cliente = accept(socket_servidor, NULL, NULL); // BLOQUEANTE
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

void* servidor_escucha(void* conexion){
	int fd_escucha = *(int*) conexion; // el contenido de la conexion
	// MULTIPLEXACION : ATENDER CLIENTES SIMULTÁNEAMETE
	while(1){
		int *fd_conexion_ptr = malloc(sizeof(int)); // malloc para que por cada cliente aceptado se lo atienda por separado
		*fd_conexion_ptr = esperar_cliente(fd_escucha);
		// una vez aceptado, se crea el hilo para manejar la solicitud:
		pthread_t thread;
		pthread_create(&thread, NULL, (void*) atender_cliente, fd_conexion_ptr);
		pthread_detach(thread); // así continúa sin esperar a que finalice el hilo
	}
	return NULL;
}

// después va personalizada para cada modulo
void* atender_cliente(void* cliente){
	int cliente_recibido = *(int*) cliente;
	while(1){
		int cod_op = recibir_operacion(cliente_recibido); // bloqueante
		switch (cod_op)
		{
		case CONEXION:
			recibir_conexion(cliente_recibido);
			break;
		case PAQUETE:
			t_list* lista = recibir_paquete(cliente_recibido);
			log_info(logger, "Me llegaron los siguientes valores:\n");
			list_iterate(lista, (void*) iterator); //esto es un mapeo
			break;
		case -1:
			log_error(logger, "Cliente desconectado.");
			close(cliente_recibido); // cierro el socket accept del cliente
			free(cliente); // libero el malloc reservado para el cliente
			pthread_exit(NULL); // solo sale del hilo actual => deja de ejecutar la función atender_cliente que lo llamó
		default:
			log_warning(logger, "Operacion desconocida.");
			break;
		}
	}
}

void iterator(char* value) {
	log_info(logger,"%s", value);
}

t_list* recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}
