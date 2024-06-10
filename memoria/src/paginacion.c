#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "paginacion.h"

void init_memoria(){
    memoria = malloc(config.tam_memoria);
}
