#include <commons/log.h>
#include <stdint.h>
#include <stdlib.h>

void log_creacion_destruccion_de_tabla_de_pagina(t_log*, uint32_t, int);
void log_acceso_a_tabla_de_pagina(t_log*, uint32_t, char*, char*);
void log_ampliacion_de_proceso(t_log*, uint32_t);
void log_reduccion_de_proceso(t_log*, uint32_t);
void log_acceso_a_espacio_de_usuario(t_log*, char*, int);
