#include "utilsServer.h"

t_log* logger;

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