#include <utils/utilsServer.h>
#include <commons/config.h>
#include <utils/buffer.h>

typedef struct{ // SIEMPRE CARGADO EN MEMORIA
    t_tabla_paginas** tabla_paginas;
    int cant_tablas;
} t_lista_tablas;

typedef struct{
    u_int32_t pid;  //PID DEL PROCESO ASOCIADO A LA TABLA
    u_int32_t tid;  // el TABLE ID jajaja
    t_pagina** paginas;
    int cant_paginas;
} t_tabla_paginas;

typedef struct{         // y donde van los datos ahre??????
    t_frame* frame;   //checkear tipo de dato
    u_int32_t offset;  //checkear tipo de dato
    //agregar un PAGE ID???
    bool presence;
} t_pagina;

typedef struct{ //para el "bitmap" de frames libres 
    u_int32_t pid;
    int pagina;
} t_frame;

typedef struct{
    u_int32_t pid;
    u_int32_t tid;
    //e_estado_proceso estado; //mm maybe?? si se usa poner el include necesario!!!
} t_pseudo_pcb;

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

extern t_lista_tablas lista_tablas;
extern u_int32_t tid; //contador para el TABLE IDENTIFICATOR 
extern t_frame** frames_libres; //"bitmap" de frames libres
extern int cant_frames = 0; //no me acuerdo para qeu hice esto
extern t_pseudo_pcb** lista_procesos; //lista de procesos con sus respectivas listas de paginas

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/


/*          NACIMIENTO            */
void init_memoria();
void init_paginacion();

t_lista_tablas* init_lista_tablas();
t_tabla_paginas* create_tabla_paginas(u_int32_t pid);
void init_bitmap_frames();


/*          MUERTE            */
void free_lista_tablas();
void free_tabla_paginas(t_tabla_paginas);
void free_pagina(t_pagina);

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

//cosas SUS
t_pagina new_pagina();
t_tabla_paginas new_tabla_paginas(u_int32_t); //recibo el pid?
bool pagina_valida(t_pagina); 
bool pagina_presente(t_pagina); //consulta pagina cargada en memoria
t_frame* consulta_marco(t_pagina); //entra pagina sale puntero al marco??
int buscar_marco_libre();
