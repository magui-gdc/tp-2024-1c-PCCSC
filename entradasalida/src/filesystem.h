#include <utils/utilsCliente.h>
#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/config.h>
#include <commons/bitarray.h>
#include <utils/buffer.h>
#include <semaphore.h>

extern t_bitarray *bitmap_bloques;

void create_bloques_dat();
void init_bitmap_bloques(); //usar bitarray de las commons
void init_metadata(); //usar commons como en los config, es un .txt