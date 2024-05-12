#include "monitores.h"

t_mqueue* monitor_NEW;
t_mqueue* monitor_READY;
t_mqueue* monitor_BLOCKED;
t_mqueue* monitor_RUNNING;
t_mqueue* monitor_EXIT;

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
}

void destruir_monitores(void){
    mqueue_destroy(monitor_NEW);
    mqueue_destroy(monitor_READY);
    mqueue_destroy(monitor_RUNNING);
    mqueue_destroy(monitor_BLOCKED);
    mqueue_destroy(monitor_EXIT);
}

/* ---- OBSOLETO CON INCLUSIÓN DE MONITORES ----
void crear_colas(){
    cola_NEW = queue_create();
    cola_READY = queue_create();
    cola_BLOCKED = queue_create();
    cola_RUNNING = queue_create();
    cola_EXIT = queue_create();
}

void destruir_colas()
{ // ver si es mejor usar queue_destroy_and_destroy_elements o esto, es opcional el parametro ese??
    queue_clean(cola_NEW);
    queue_destroy(cola_NEW);
    queue_clean(cola_READY);
    queue_destroy(cola_READY);
    queue_clean(cola_BLOCKED);
    queue_destroy(cola_BLOCKED);
    queue_clean(cola_RUNNING);
    queue_destroy(cola_RUNNING);
    queue_clean(cola_EXIT);
    queue_destroy(cola_EXIT);
    free(cola_NEW);
    free(cola_READY);
    free(cola_BLOCKED);
    free(cola_RUNNING);
    free(cola_EXIT);
}
*/

/*
void *buscar_pcb_por_pid(uint32_t pid_buscado)
{
    bool comparar_pid(void *elemento){
        t_pcb *pcb = (t_pcb *)elemento;
        return pcb->pid == pid_buscado;
    }
    return list_find(pcb_list, comparar_pid); // si no funciona pasarlo como puntero a funcion
}

t_queue *obtener_cola(uint32_t pid_buscado){
    // t_pcb *pcb = dictionary_get(pcb_dictionary, pid_buscado);
    t_pcb *pcb_encontrado = (t_pcb *)buscar_pcb_por_pid(pid_buscado);
    switch (pcb_encontrado->estado)
    {
    case NEW:
        return cola_NEW;
    case READY:
        return cola_READY;
    case BLOCKED:
        return cola_BLOCKED;
    case RUNNING:
        return cola_RUNNING;
    case EXIT:
        return cola_EXIT;
    default:
        return 0; // y mensaje de error
    }
    return NULL;
}*/
