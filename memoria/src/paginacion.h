#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/buffer.h>

typedef struct{ // SIEMPRE CARGADO EN MEMORIA
    t_tabla_paginas** tabla_paginas;
    int cant_tablas;
    bool presence;
} t_lista_tablas;

typedef struct{
    u_int32_t pid;  //PID DEL PROCESO ASOCIADO A LA TABLA
    t_pagina** paginas;
} t_tabla_paginas;

typedef struct{
    t_frame* frame;   //checkear tipo de dato
    u_int32_t offset;  //checkear tipo de dato
    bool presence;
} t_pagina;

typedef struct{
    u_int32_t pid;
    int pagina;
} t_frame;

// inicializacion de las estructuras necesarias para la paginacion
void init_memoria();
void init_paginacion();
void init_lista_tablas();
void free_lista_tablas();
void free_tabla_paginas(t_tabla_paginas);
void free_pagina(t_pagina);
void init_bitmap_frames();

//cosas SUS
t_pagina new_pagina();
t_tabla_paginas new_tabla_paginas(u_int32_t); //recibo el pid?
bool pagina_valida(t_pagina); 
bool pagina_presente(t_pagina); //consulta pagina cargada en memoria
t_frame* consulta_marco(t_pagina); //entra pagina sale puntero al marco??
int buscar_marco_libre();
