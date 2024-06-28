#include "logs.h"

void log_iniciar_proceso(t_log* logger, uint32_t pid){
    log_info(logger, "Se crea el proceso %d en NEW", pid);
}
void log_cambio_estado_proceso(t_log* logger, uint32_t pid, char* estado_anterior, char* estado_actual){
    log_info(logger, "PID: %d - Estado Anterior: %s - Estado Actual: %s", pid, estado_anterior, estado_actual);
}

void log_finaliza_proceso(t_log* logger, uint32_t pid, char* motivo){
    log_info(logger, "Finaliza el proceso: %d - Motivo: %s", pid, motivo);
}

void log_desalojo_fin_de_quantum(t_log* logger, uint32_t pid){
    log_info(logger, "PID: %d - Desalojado por fin de Quantum", pid);
}

void log_ingreso_ready(t_log* logger, t_mqueue* cola_ready){
    char* listado_pid = (char*)malloc(128);
    sem_wait(&(cola_ready->mutex));
    int max = queue_size(cola_ready->cola); 
    char* pid_str = (char*)malloc(20);
    for(int i = 0; i<max; i++){   
        t_pcb* elemento = list_get(cola_ready->cola->elements, i);
        sprintf(pid_str, "%d", elemento->pid);
        if(i != 0) strcat(listado_pid, ", ");
        strcat(listado_pid, pid_str);
    }
    sem_post(&(cola_ready->mutex));
    
    log_info(logger, "Cola Ready %p: [%s]", (void*)cola_ready, listado_pid); // TODO: CORREGIR
    free(listado_pid);
    free(pid_str);
}

void log_bloqueo_proceso(t_log* logger, uint32_t pid, char* motivo){
    log_info(logger, "PID: %u - Bloqueado por: %s", pid, motivo);
}