#include "logs.h"

void log_iniciar_proceso(t_log* logger, uint32_t pid){
    char* mensaje = (char*)malloc(128);
    sprintf(mensaje, "Se crea el proceso %d en NEW", pid);
    log_info(logger, "%s", mensaje);
    free(mensaje);
}
void log_cambio_estado_proceso(t_log* logger, uint32_t pid, char* estado_anterior, char* estado_actual){
    char* mensaje = (char*)malloc(128);
    sprintf(mensaje, "PID: %d - Estado Anterior: %s - Estado Actual: %s", pid, estado_anterior, estado_actual);
    log_info(logger, "%s", mensaje);
    free(mensaje);
}

void log_creacion_destruccion_de_tabla_de_pagina(t_log* logger, uint32_t pid, int tamanio){
    log_info(logger, "PID: %u - Tamaño: %d", pid, tamanio);
};


void log_acceso_a_tabla_de_pagina(t_log* logger, uint32_t proceso, int pagina, uint32_t marco){
    log_info(logger, "PID: %u - Pagina: %d - Marco: %u", proceso, pagina, marco);
};

void log_ampliacion_de_proceso(t_log* logger, uint32_t pid){

};

void log_reduccion_de_proceso(t_log* logger, uint32_t pid){

};

void log_acceso_a_espacio_de_usuario(t_log* logger, char* tipo, int tamanio){

};
