#include <commons/log.h>
#include <stdint.h>
#include <stdlib.h>

void log_operacion(t_log* logger, uint32_t pid, char* instruccion);
void log_crear_archivo(t_log* logger, uint32_t pid, char* nombre_archivo);
void log_eliminar_archivo(t_log* logger, uint32_t pid, char* nombre_archivo);
void log_truncar_archivo(t_log* logger, uint32_t pid, char* nombre_archivo, int tamanio);
void log_leer_archivo(t_log* logger, uint32_t pid, char* nombre_archivo, int tamanio, void* puntero);
void log_escribir_archivo(t_log* logger, uint32_t pid, char* nombre_archivo, int tamanio, void* puntero);
void inicio_compactacion(t_log* logger, uint32_t pid); 
void fin_compactacion(t_log* logger, uint32_t pid); 


