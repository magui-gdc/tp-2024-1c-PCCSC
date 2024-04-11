#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

int main(int argc, char* argv[]) {
    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);
    
    int socket_servidor = iniciar_servidor("4444");

    log_info(logger, "Server iniciado bip bop bop bip");

    esperar_cliente(socket_servidor);

    decir_hola("Kernel");
    return 0;
}
