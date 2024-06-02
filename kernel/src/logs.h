#include <commons/log.h>
#include <stdint.h>
#include <stdlib.h>
#include "monitores.h"
#include <string.h>
#include "main.h"

void log_iniciar_proceso(t_log* logger, uint32_t pid);
void log_cambio_estado_proceso(t_log* logger, uint32_t pid, char* estado_anterior, char* estado_actual);
void log_desalojo_fin_de_quantum(t_log* logger, uint32_t pid);
void log_finaliza_proceso(t_log* logger, uint32_t pid, char* motivo);
void log_bloqueo_proceso(t_log*, char*);
void log_ingreso_ready(t_log* logger, t_mqueue* cola_ready);