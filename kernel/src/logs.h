#include <commons/log.h>
#include <stdint.h>
#include <stdlib.h>

void log_iniciar_proceso(t_log*, uint32_t);
void log_cambio_estado_proceso(t_log*, uint32_t, char*, char*);
void log_desalojo_fin_de_quantum(t_log*, uint32_t);
void log_finaliza_proceso(t_log*, uint32_t, char*);
void log_bloqueo_proceso(t_log*, char*);
void log_ingreso_ready(t_log*/*, array de PID*/);