#include "logs.h"

void log_operacion(t_log* logger, uint32_t pid, char* instruccion){
    log_info(logger, "PID: %d - Operacion: %s", pid, instruccion);
}

void log_crear_archivo(t_log* logger, uint32_t pid, char* nombre_archivo){
    log_info(logger, "PID: %d - Crear Archivo: %s", pid, nombre_archivo);
}

void log_eliminar_archivo(t_log* logger, uint32_t pid, char* nombre_archivo){
    log_info(logger, "PID: %d - Eliminar Archivo: %s", pid, nombre_archivo);
}

void log_truncar_archivo(t_log* logger, uint32_t pid, char* nombre_archivo, int tamanio){
    log_info(logger, "PID: %d - Truncar Archivo: %s - Tamaño: %d", pid, nombre_archivo, tamanio);
}

void log_leer_archivo(t_log* logger, uint32_t pid, char* nombre_archivo, int tamanio, uint32_t puntero){
    log_info(logger, "PID: %u - Leer Archivo: %s - Tamaño a Leer: %d - Puntero Archivo: %u", pid, nombre_archivo, tamanio, puntero);
}

void log_escribir_archivo(t_log* logger, uint32_t pid, char* nombre_archivo, int tamanio, uint32_t puntero){
    log_info(logger, "PID: %u - Escribir Archivo: %s - Tamaño a Escribir: %d - Puntero Archivo: %u", pid, nombre_archivo, tamanio, puntero);
}

void inicio_compactacion(t_log* logger, uint32_t pid){
    log_info(logger, "PID: %d - Inicio Compactación.", pid);
}
void fin_compactacion(t_log* logger, uint32_t pid){
    log_info(logger, "PID: %d - Fin Compactación.", pid);
}