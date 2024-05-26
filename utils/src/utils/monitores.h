#include <stdio.h>
#include <stdlib.h>
#include <commons/collections/queue.h>
#include <semaphore.h>

typedef struct {
    t_queue* cola;
    sem_t mutex;
} t_mqueue; // MONITOR DE COLAS

#ifndef MONITORES_H
#define MONITORES_H
extern t_mqueue* monitor_NEW;
extern t_mqueue* monitor_READY;
extern t_mqueue* monitor_BLOCKED;
extern t_mqueue* monitor_RUNNING;
extern t_mqueue* monitor_EXIT;
extern t_mqueue* monitor_INTERRUPCIONES;
#endif

// --------- MONITORES (sincronizacion) --------- //
t_mqueue* mqueue_create(void);
void mqueue_destroy(t_mqueue* monitor);
void* mqueue_pop(t_mqueue* monitor); // escritura
void mqueue_push(t_mqueue* monitor, void* elemento); // escritura
void* mqueue_peek(t_mqueue* monitor); // lectura
bool mqueue_is_empty(t_mqueue* monitor); // lectura

void crear_monitores(void);
void destruir_monitores(void);

// --------- MANEJO DE COLECCIONES KERNEL --------- //
//void crear_colas(void); // OBSOLETO CON USO DE MONITORES
//void destruir_colas();
//t_queue *obtener_cola(uint32_t pid_buscado); //ANALIZAR CON PASE A MONITORES ¿?
//void* buscar_pcb_por_pid(uint32_t);
//t_queue* obtener_cola(uint32_t);
