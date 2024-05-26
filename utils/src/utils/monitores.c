#include "monitores.h"

t_mqueue* monitor_NEW;
t_mqueue* monitor_READY;
t_mqueue* monitor_BLOCKED;
t_mqueue* monitor_RUNNING;
t_mqueue* monitor_EXIT;
t_mqueue* monitor_INTERRUPCIONES;

t_mqueue* mqueue_create(void) {
    t_mqueue* monitor = malloc(sizeof(t_mqueue));
    if (monitor == NULL) {
        exit(EXIT_FAILURE);
    }
    monitor->cola = queue_create(); 
    sem_init(&monitor->mutex, 0, 1);
    return monitor;
}

void mqueue_destroy(t_mqueue* monitor) {
    queue_destroy(monitor->cola); 
    sem_destroy(&monitor->mutex);
    free(monitor); // Libera la memoria del monitor
}

void* mqueue_pop(t_mqueue* monitor) {
    sem_wait(&monitor->mutex);
    void* elemento = queue_pop(monitor->cola);
    sem_post(&monitor->mutex);
    return elemento;
}

void mqueue_push(t_mqueue* monitor, void* elemento) {
    sem_wait(&monitor->mutex); 
    queue_push(monitor->cola, elemento);
    sem_post(&monitor->mutex);
}

void* mqueue_peek(t_mqueue* monitor) {
    sem_wait(&monitor->mutex); 
    void* elemento = queue_peek(monitor->cola); 
    sem_post(&monitor->mutex); 
    return elemento;
}

bool mqueue_is_empty(t_mqueue* monitor){
    sem_wait(&monitor->mutex);
    bool respuesta = queue_is_empty(monitor->cola);
    sem_post(&monitor->mutex);
    return respuesta;
}

void crear_monitores(void){
    monitor_NEW = mqueue_create();
    monitor_READY = mqueue_create();
    monitor_RUNNING = mqueue_create(); //TODO: NO DEBERÍA SER UNA COLA => se supone que se ejecuta de a un proceso y que el acceso a RUNNING está controlado desde el planificador de corto plazo (ya sincronizado por semáforos) 
    monitor_BLOCKED = mqueue_create();
    monitor_EXIT = mqueue_create();
    monitor_INTERRUPCIONES = mqueue_create();
}

void destruir_monitores(void){
    mqueue_destroy(monitor_NEW);
    mqueue_destroy(monitor_READY);
    mqueue_destroy(monitor_RUNNING);
    mqueue_destroy(monitor_BLOCKED);
    mqueue_destroy(monitor_EXIT);
    mqueue_destroy(monitor_INTERRUPCIONES);
}