#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

int main(int argc, char* argv[]) {

    char* puerto;
    t_config* config = iniciar_config("kernel.config", &puerto);
    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);
    
    decir_hola("Kernel");

    int socket_servidor = iniciar_servidor(puerto);
    log_info(logger, puerto);
    log_info(logger, "Server iniciado bip bop bop bip");

    int cliente;  
    while((cliente = esperar_cliente(socket_servidor)) != -1){
        int cod_op = recibir_operacion(cliente);
        switch (cod_op)
        {
        case CONEXION:
            recibir_conexion(cliente);
            break;
        case -1:
            log_error(logger, "cliente desconectado");
            break;
        default:
            log_warning(logger, "Operacion desconocida.");
            break;
        }
    }
    return EXIT_SUCCESS;
}
