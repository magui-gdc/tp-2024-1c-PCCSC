#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

/* TODO:
hilos de largo plazo (uno NEW->READY otro ALL->EXIT)
atender_cliente por cada modulo
corto_plazo
semaforos (plantear: info en lucid.app)
list_find funcion para comparar elementos
comunicacion kernel - memoria - cpu (codigos de operacion, sockets, leer instrucciones y serializar)
instruccion ENUM con una funcion que delegue segun ENUM una funcion a ejecutar
memoria para leer en proceso (ver commmons)
*/

config_struct config;

t_list *pcb_list; // lista dinamica que contiene los PCB de los procesos creados
uint32_t pid;     // PID: contador para determinar el PID de cada proceso creado

uint8_t PLANIFICACION_PAUSADA;
prcs_fin FINALIZACION;

t_queue *cola_NEW;
t_queue *cola_READY;
t_queue *cola_BLOCKED;
t_queue *cola_RUNNING;
t_queue *cola_EXIT;

sem_t mutex_planificacion_pausada, mutex_cola_ready, contador_grado_multiprogramacion, orden_planificacion;

int main(int argc, char *argv[]){

    int conexion_cpu_dispatch, conexion_memoria, conexion_cpu_interrupt;
    pthread_t thread_kernel_servidor, thread_kernel_consola, thread_planificador_corto_plazo, thread_planificador_largo_plazo;

    // ------------ ARCHIVOS CONFIGURACION + LOGGER ------------
    t_config *archivo_config = iniciar_config("kernel.config");
    cargar_config_struct_KERNEL(archivo_config);
    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);

    // -- INICIALIZACION VARIABLES GLOBALES -- //
    pid = 1;
    PLANIFICACION_PAUSADA = 0;

    // -- INICIALIZACION SEMAFOROS -- //
    sem_init(&mutex_planificacion_pausada,0,1);
    sem_init(&mutex_cola_ready,0,1);
    sem_init(&orden_planificacion,0,0);
    sem_init(&contador_grado_multiprogramacion, 0, config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION"));


    decir_hola("Kernel");

    // ------------ CONEXION CLIENTE - SERVIDORES ------------
    // conexion puertos cpu
    conexion_cpu_dispatch = crear_conexion(config.ip_cpu, config.puerto_cpu_dispatch);
    log_info(logger, "se conecta a CPU puerto DISPATCH");
    enviar_conexion("Kernel a DISPATCH", conexion_cpu_dispatch);

    conexion_cpu_interrupt = crear_conexion(config.ip_cpu, config.puerto_cpu_interrupt);
    log_info(logger, "se conecta a CPU puerto INTERRUPT");
    enviar_conexion("Kernel a INTERRUPT", conexion_cpu_interrupt);

    // conexion memoria
    conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);
    log_info(logger, "se conecta a MEMORIA");
    enviar_conexion("Kernel", conexion_memoria);

    // ------------ CONEXION SERVIDOR - CLIENTES ------------
    int socket_servidor = iniciar_servidor(config.puerto_escucha);
    // log_info(logger, config.puerto_escucha);
    log_info(logger, "Server KERNEL iniciado");

    // ------------ HILOS ------------
    // hilo con MULTIPLEXACION a Interfaces I/O
    if (pthread_create(&thread_kernel_servidor, NULL, servidor_escucha, &socket_servidor) != 0)
    {
        log_error(logger, "No se ha podido crear el hilo para la conexion con interfaces I/O");
        exit(EXIT_FAILURE);
    }
    // hilo para recibir mensajes por consola
    if (pthread_create(&thread_kernel_consola, NULL, consola_kernel, archivo_config) != 0)
    {
        log_error(logger, "No se ha podido crear el hilo para la consola kernel");
        exit(EXIT_FAILURE);
    }
    // hilo para planificacion a corto plazo (READY A EXEC)
    if (pthread_create(&thread_planificador_corto_plazo, NULL, planificar_corto_plazo, archivo_config) != 0)
    {
        log_error(logger, "No se ha podido crear el hilo para el planificador corto plazo");
        exit(EXIT_FAILURE);
    }
    // hilo para planificacion a largo plazo (NEW A READY)
    if (pthread_create(&thread_planificador_largo_plazo, NULL, planificar_largo_plazo, archivo_config) != 0)
    {
        log_error(logger, "No se ha podido crear el hilo para el planificador largo plazo");
        exit(EXIT_FAILURE);
    }

    pthread_join(thread_kernel_servidor, NULL);
    pthread_join(thread_kernel_consola, NULL);
    pthread_join(thread_planificador_corto_plazo, NULL);
    pthread_join(thread_planificador_largo_plazo, NULL);
    
    sem_destroy(&mutex_cola_ready);
    sem_destroy(&mutex_cola_ready);
    sem_destroy(&contador_grado_multiprogramacion);
    sem_destroy(&orden_planificacion);

    log_destroy(logger);
    config_destroy(archivo_config);
    liberar_conexion(conexion_memoria);
    liberar_conexion(conexion_cpu_dispatch);
    liberar_conexion(conexion_cpu_interrupt);

    return EXIT_SUCCESS;
}

// ------------ DEFINICION FUNCIONES KERNEL ------------

void cargar_config_struct_KERNEL(t_config *archivo_config)
{
    config.puerto_escucha = config_get_string_value(archivo_config, "PUERTO_ESCUCHA");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
    config.ip_cpu = config_get_string_value(archivo_config, "IP_CPU");
    config.puerto_cpu_dispatch = config_get_string_value(archivo_config, "PUERTO_CPU_DISPATCH");
    config.puerto_cpu_interrupt = config_get_string_value(archivo_config, "PUERTO_CPU_INTERRUPT");
    config.algoritmo_planificacion = config_get_string_value(archivo_config, "ALGORITMO_PLANIFICACION");
    config.quantum = config_get_int_value(archivo_config, "QUANTUM");
    // TODO: a partir de aca, en realidad no conviene guardarlos, porque los valores de estas claves se pueden modificar desde distintos hilos!
    // por ahora lo comento para analizar despues
    /*config.recursos = config_get_array_value(archivo_config, "RECURSOS" );
    config.instancias = config_get_array_value(archivo_config, "INSTANCIAS");
    config.grado_multiprogramacion = config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION");*/
}

// definicion funcion hilo consola
void *consola_kernel(void *archivo_config)
{
    char *leido;

    while (1)
    {
        leido = readline("> ");

        // Verificar si se ingresó algo
        if (strlen(leido) == 0)
        {
            free(leido);
            break;
        }

        char **tokens = string_split(leido, " ");
        char *comando = tokens[0];
        if (comando != NULL)
        {
            if (strcmp(comando, "EJECUTAR_SCRIPT") == 0 && string_array_size(tokens) >= 2)
            {
                char *path = tokens[1];
                if (strlen(path) != 0 && path != NULL)
                {
                    // comprobar existencia de archivo en ruta relativa
                    // ejecutar_script(path);
                    printf("path ingresado (ejecutar_script): %s\n", path);
                }
            }
            else if (strcmp(comando, "INICIAR_PROCESO") == 0 && string_array_size(tokens) >= 2)
            {
                char *path = tokens[1];
                uint8_t pid_proceso_iniciado;
                if (strlen(path) != 0 && path != NULL)
                {
                    // solicitar creacion a memoria de proceso
                    // si se crea proceso, iniciar largo plazo

                    pid_proceso_iniciado = iniciar_proceso(path);
                    // ver funcion para comprobar existencia de archivo en ruta relativa en MEMORIA ¿acá o durante ejecución? => revisar consigna

                    // lo agrega en la cola NEW --> consultada desde planificador_largo_plazo, evaluar semáforo.

                    printf("path ingresado (iniciar_proceso): %s\n", path);
                }
            }
            else if (strcmp(comando, "FINALIZAR_PROCESO") == 0 && string_array_size(tokens) >= 2)
            {
                char *pid = tokens[1];
                if (strlen(pid) != 0 && pid != NULL && atoi(pid) > 0)
                {
                    // finalizar_proceso(pid);
                    FINALIZACION.FLAG_FINALIZACION = true; // falta que el cpu y los errores puedan activar el flag también
                    FINALIZACION.PID = pid;
                    printf("pid ingresado (finalizar_proceso): %s\n", pid);
                }
            }
            else if (strcmp(comando, "MULTIPROGRAMACION") == 0 && string_array_size(tokens) >= 2)
            {
                char *valor = tokens[1];
                if (strlen(valor) != 0 && valor != NULL && atoi(valor) > 0)
                {
                    config_set_value((t_config *)archivo_config, "GRADO_MULTIPROGRAMACION", valor);
                    // TODO: WAIT MUTEX ACA!! => porque esta informacion es consultada desde PLANIFICACION_LARGO_PLAZO
                    //contador_grado_multiprogramacion = config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION");
                    sem_init(&contador_grado_multiprogramacion, 0, config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION"));
                    config_save((t_config *)archivo_config);
                    // TODO: SIGNAL MUTEX ACA!! => porque esta informacion es consultada desde PLANIFICACION_LARGO_PLAZO
                    printf("grado multiprogramacion cambiado a %s\n", valor);
                }
            }
            else if (strcmp(comando, "DETENER_PLANIFICACION") == 0){   
                sem_wait(&mutex_planificacion_pausada);
                PLANIFICACION_PAUSADA = 1; // escritura
                sem_post(&mutex_planificacion_pausada);
                printf("detener planificacion\n");
            }
            else if (strcmp(comando, "INICIAR_PLANIFICACION") == 0)
            {
                sem_wait(&mutex_planificacion_pausada);
                PLANIFICACION_PAUSADA = 0; // escritura
                sem_post(&mutex_planificacion_pausada);
                printf("iniciar planificacion\n");
            }
            else if (strcmp(comando, "PROCESO_ESTADO") == 0)
            {
                // estados_procesos()
                printf("estados de los procesos\n");
            }
        }
        string_array_destroy(tokens);
        free(leido);
    }

    return NULL;
}

void *planificar_corto_plazo(void *archivo_config)
{
    while (1)
    {
        // agregar SEMAFORO planificacion_pausada
        // PLANIFICACION corto plazo
    }
}

void *planificar_largo_plazo(void *archivo_config)
{
    pthread_t thread_NEW_READY, thread_ALL_EXIT;

    pthread_create(thread_NEW_READY, NULL, planificar_new_to_ready, archivo_config);
    pthread_create(thread_ALL_EXIT, NULL, planificar_all_to_exit, NULL);

    pthread_detach(thread_NEW_READY);
    pthread_detach(thread_NEW_READY);
   
}

void *planificar_new_to_ready(void *archivo_config)
{
    while (1){
        sem_wait(&mutex_planificacion_pausada);
        if (!PLANIFICACION_PAUSADA){ //lectura
            sem_post(&mutex_planificacion_pausada);
            // NEW -> READY ( TODO: delegar en  SUBhilo)
            if (queue_size(cola_NEW) != 0 /*&& grado de multiprogamacion lo permite*/){
                sem_wait(&contador_grado_multiprogramacion)
                queue_push(cola_READY, queue_peek(cola_NEW));
                queue_pop(cola_NEW);
            }
        } else {
            sem_post(&mutex_planificacion_pausada);
        }
    }
}

void *planificar_all_to_exit(void *args){
    while (1)
    {
        if (FINALIZACION.FLAG_FINALIZACION)
        {
            if (obtener_cola(FINALIZACION.PID) == cola_RUNNING)
            {
                // mensaje de interrupt al cpu
                queue_push(cola_EXIT, queue_peek(cola_RUNNING));
                queue_pop(cola_RUNNING);
            }
            else
            {
                queue_push(cola_EXIT, queue_peek(obtener_cola(FINALIZACION.PID)));
                queue_pop(obtener_cola(FINALIZACION.PID));
            }
            if (obtener_cola(FINALIZACION.PID) != cola_NEW)
            {
                // habilitar espacio de multiprogramación (esto será muy probablemente un semáforo)
            }
            // poner error si se encuentra en la cola_EXIT
        }
    }
}

void crear_colas()
{
    cola_NEW = queue_create();
    cola_READY = queue_create();
    cola_BLOCKED = queue_create();
    cola_RUNNING = queue_create();
    cola_EXIT = queue_create();
}

t_queue *obtener_cola(uint32_t pid)
{
    t_pcb *pcb = list_find(pcb_list, /* ???? no se com hacer esta func*/);
    // podemos hacer que tras encontrar el pcb, en vez de hacer los if, obtenemos su estado (pcb->estado)
    // y en base a eso hacemos un switch ya que la variable estado es enum

    // Tincho, dejalo comentado y lo dejamos asi por ahora, despues se lo mostramos a los chicos, y si quieren lo hacemos con switch, pq no sabemos usar list_find xd

    switch (pcb->estado)
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
        return 0 // y mensaje de error
    }

    /*if(queue_peek(cola_NEW) == pcb){
        return cola_NEW
    }
    if(queue_peek(cola_READY) == pcb){
        return cola_READY
    }
    if(queue_peek(cola_BLOCKED) == pcb){
        return cola_BLOCKED
    }
    if(queue_peek(cola_RUNNING) == pcb){
        return cola_RUNNING
    }
    if(queue_peek(cola_EXIT) == pcb){
        return cola_EXIT
    }*/
    // <---- solución falopa
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

//////////////////////////////////////////////////////////////////////////////////////////////////////

// esto debería ir en memoria, y se ejecuta despues de verificar que el path existe.
uint32_t iniciar_proceso(void *arg)
{

    t_pcb *proceso;

    proceso->estado = NEW;
    proceso->quantum = 0;
    proceso->program_counter = 0; // arranca en 0?
    proceso->pid = pid;
    proceso->registros = obtener_registros(/*arg? registros cpu???*/);

    pid++;

    queue_push(cola_NEW, proceso);
    list_add(pcb_list, proceso);

    return proceso->pid;
}

t_registros_cpu obtener_registros()
{
    t_registros_cpu registros
        registros.PC = &malloc(sizeof(uint32_t));
    registros.AX = &malloc(sizeof(uint8_t));
    registros.BX = &malloc(sizeof(uint8_t));
    registros.CX = &malloc(sizeof(uint8_t));
    registros.DX = &malloc(sizeof(uint8_t));
    registros.EAX = &malloc(sizeof(uint32_t));
    registros.EBX = &malloc(sizeof(uint32_t));
    registros.ECX = &malloc(sizeof(uint32_t));
    registros.EDX = &malloc(sizeof(uint32_t));
    registros.SI = &malloc(sizeof(uint32_t));
    registros.DI = &malloc(sizeof(uint32_t));
}

/*
bool comparacion(uint8_t _pid) {
        t_pcb* pcb = (t_pcb*)elemento;
        uint8_t pid_busqueda = _pid;
        return pcb->pid == pid_busqueda;
}

void finalizar_proceso(char* pid_buscado){

    struct {
        uint8_t _pid = (uint8_t)atoi(pid_buscado);
        t_pcb* elemento;
    }
    t_pcb* proc = list_find(pcb_list, comparacion);
    free(proc);

}
*/

//////////////////////////////////////////////////////////////////////////////////////////////////////