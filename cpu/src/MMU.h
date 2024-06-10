/*#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>

#include <string.h>
#include <commons/config.h>
#include "enums.h"*/

#include "main.h"
#include <commons/collections/list.h>

typedef struct {
    u_int32_t pid;
    int pagina;
    int marco;
}t_tlb;


void solicitar_frame_a_memoria();
int buscar_en_tlb (int);