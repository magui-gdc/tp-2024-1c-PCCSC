#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "logs.h" // logs incluye el main.h de kernel

/* TODO:
REVISAR BIEN QUE FUNCIONE OK COMANDOS DE MULTIPROGRAMACION Y DE INICIAR/DETENER PLANIFICACION (este commit está listo para probar eso)
hilos de largo plazo (uno NEW->READY otro ALL->EXIT) listo.
atender_cliente por cada modulo
corto_plazo
semaforos (plantear: info en lucid.app) listo.
list_find funcion para comparar elementos listo. (¡¡FALTA PROBARLO!!)
comunicacion kernel - memoria - cpu (codigos de operacion, sockets, leer instrucciones y serializar)
instruccion ENUM con una funcion que delegue segun ENUM una funcion a ejecutar
memoria para leer en proceso (ver commmons)
*/

// ---------- CONSTANTES ---------- //
const char *estado_proceso_strings[] = {
    "NEW", 
    "READY", 
    "RUNNING", 
    "BLOCKED", 
    "EXIT"
};

config_struct config;
t_list *pcb_list;
// t_dictionary *pcb_dictionary; // dictionario dinamica que contiene los PCB de los procesos creados
uint32_t pid; // PID: contador para determinar el PID de cada proceso creado
fc_puntero algoritmo_planificacion;

uint8_t PLANIFICACION_PAUSADA;
prcs_fin FINALIZACION;

t_queue *cola_READY_VRR;

sem_t mutex_planificacion_pausada, contador_grado_multiprogramacion, orden_planificacion, listo_proceso_en_running, orden_proceso_exit, cambio_estado_desalojo;
sem_t mutex_cambio_estado; // TODO: agregarlo donde corresponda (es un semáforo que controla que no se pase un proceso de una cola a otra mientras se está realizando una operación de FINALIZAR_PROCESO)
int conexion_cpu_dispatch, conexion_memoria, conexion_cpu_interrupt;

int main(int argc, char *argv[])
{
    // ------------ DECLARACION HILOS HIJOS PRINCIPALES ------------ //
    pthread_t thread_kernel_servidor, thread_kernel_consola, thread_planificador_corto_plazo, thread_planificador_largo_plazo;

    // ------------ ARCHIVOS CONFIGURACION + LOGGER ------------
    t_config *archivo_config = iniciar_config("kernel.config");
    cargar_config_struct_KERNEL(archivo_config);
    algoritmo_planificacion = obtener_algoritmo_planificacion();
    logger = log_create("log.log", "Servidor", 0, LOG_LEVEL_DEBUG);

    // -- INICIALIZACION VARIABLES GLOBALES -- //
    pid = 1;
    PLANIFICACION_PAUSADA = 0;
    pcb_list = list_create();
    crear_monitores(); // crear_colas();

    // -- INICIALIZACION SEMAFOROS -- //
    sem_init(&mutex_planificacion_pausada, 0, 1);
    sem_init(&orden_planificacion, 0, 0);
    sem_init(&contador_grado_multiprogramacion, 0, config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION"));
    sem_init(&listo_proceso_en_running, 0, 0);
    sem_init(&orden_proceso_exit, 0, 0);
    sem_init(&cambio_estado_desalojo, 0, 1);
    sem_init(&mutex_cambio_estado,0,1);

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

    destruir_monitores();

    sem_destroy(&mutex_planificacion_pausada);
    sem_destroy(&contador_grado_multiprogramacion);
    sem_destroy(&orden_planificacion);
    sem_destroy(&listo_proceso_en_running);
    sem_destroy(&cambio_estado_desalojo);
    sem_destroy(&orden_proceso_exit);
    sem_destroy(&mutex_cambio_estado);

    log_destroy(logger);
    config_destroy(archivo_config);
    liberar_conexion(conexion_memoria);
    liberar_conexion(conexion_cpu_dispatch);
    liberar_conexion(conexion_cpu_interrupt);

    return EXIT_SUCCESS;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// --------------------------- DEFINICION FUNCIONES KERNEL -----------------------------

// -------------------- INICIO FCS. DE CONFIGURACION --------------------
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
}
// -------------------- FIN FCS. DE CONFIGURACION --------------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// -------------------- INICIO FCS. DE HILOS KERNEL --------------------
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
                    consola_interactiva(path);
                    printf("path ingresado (ejecutar_script): %s\n", path);
                }
            } // INICIAR_PROCESO /carpetaProcesos/proceso1
            else if (strcmp(comando, "INICIAR_PROCESO") == 0 && string_array_size(tokens) >= 2){
                char *path = tokens[1];
                t_paquete* paquete_proceso = crear_paquete();
                if (strlen(path) != 0 && path != NULL){   
                    agregar_a_paquete(paquete_proceso, path, sizeof(path));
                    paquete_proceso->codigo_operacion = INICIAR_PROCESO;
                    enviar_paquete(paquete_proceso, conexion_memoria);
                    // solicitar creacion a memoria de proceso
                    // si se crea proceso, iniciar largo plazo

                    iniciar_proceso(path);
                    // ver funcion para comprobar existencia de archivo en ruta relativa en MEMORIA ¿acá o durante ejecución? => revisar consigna
                    printf("path ingresado (iniciar_proceso): %s\n", path);
                }
            }
            else if (strcmp(comando, "FINALIZAR_PROCESO") == 0 && string_array_size(tokens) >= 2){
                char *pid_char = tokens[1], *endptr;
                uint32_t valor_uint_pid = strtoul(pid_char, &endptr, 10);
                if (strlen(pid_char) != 0 && pid_char != NULL && valor_uint_pid > 0 && *endptr == '\0'){
                    printf("pid ingresado (finalizar_proceso): %s\n", pid_char);
                    
                    // 1. detengo la planificación: así los procesos no cambian su estado ni pasan de una cola a otra!
                    sem_wait(&mutex_planificacion_pausada);
                    int planificacion_registrada = PLANIFICACION_PAUSADA;
                    PLANIFICACION_PAUSADA = 1;
                    sem_post(&mutex_planificacion_pausada);

                    // 2. pregunto por su estado
                    t_pcb* proceso_buscado = buscar_pcb_por_pid(valor_uint_pid);

                    if (proceso_buscado){
                        printf("obtengo proceso %u, en estado: %s\n", proceso_buscado->pid, (char *)estado_proceso_strings[proceso_buscado->estado] );

                        // 3. trabajo la solicitud de finalización según su estado
                        if(proceso_buscado->estado != EXIT){ // EXIT: Si ya está en EXIT no se debe procesar la solicitud
                            
                            // RUNNING -> puede estar todavía en CPU o justo siendo desalojado (caso más borde pero bueno): 
                            if(proceso_buscado->estado == RUNNING){ 
                                // cuando el proceso está en running hay que avisarle a cpu que lo desaloje o a corto plazo que no procese su mensaje de desalojo, según donde esté
                                if(proceso_buscado->desalojo == SIN_DESALOJAR){ 
                                    // cuando el proceso está todavía en CPU mando interrupción
                                    t_pic interrupt = {proceso_buscado->pid, FINALIZAR_PROCESO, 0};
                                    enviar_interrupcion_a_cpu(interrupt);
                                    sem_wait(&cambio_estado_desalojo);
                                    proceso_buscado->desalojo = PEDIDO_FINALIZACION;
                                    sem_post(&cambio_estado_desalojo); // por las dudas de que ya haya desalojado antes de recibir la INT para que se lo finalice igual
                                    log_info(logger, "mandaste interrupcion con FINALIZAR_PROCESO");
                                } else if (proceso_buscado->desalojo == DESALOJADO) {
                                    // cuando el proceso fue desalojado de CPU pero todavía no se trató su motivo de desalojo
                                    sem_wait(&cambio_estado_desalojo);
                                    proceso_buscado->desalojo = PEDIDO_FINALIZACION; 
                                    sem_post(&cambio_estado_desalojo);
                                    log_info(logger, "PEDIDO_FINALIZACION");
                                    // la idea es que desde corto plazo sólo se procese su motivo de desalojo cuando no haya un pedido de finalizacion! y si hay un pedido de finalización, se mande el proceso a EXIT!
                                }
                                // en el caso de RUNNING, el proceso de desalojo (recibir el código de operación) se maneja desde CORTO PLAZO!
                            } else { 
                                // para cualquier otro estado: se lo saca de su cola actual y se lo pasa a EXIT desde acá!
                                t_pcb* proceso_encontrado = extraer_proceso(proceso_buscado->pid, proceso_buscado->estado); // lo saco de su cola actual
                                mqueue_push(monitor_EXIT, proceso_encontrado); // lo agrego a EXIT
                                sem_post(&orden_proceso_exit);
                            }
                        }
                    } else
                        printf("pid no existente\n");

                    // 4. inicio la planificacion
                    sem_wait(&mutex_planificacion_pausada);
                    PLANIFICACION_PAUSADA = planificacion_registrada; // la vuelvo a setear a su valor anterior
                    sem_post(&mutex_planificacion_pausada);
                
                }
            }
            else if (strcmp(comando, "MULTIPROGRAMACION") == 0 && string_array_size(tokens) >= 2)
            {
                char *valor = tokens[1];
                if (strlen(valor) != 0 && valor != NULL && atoi(valor) > 0)
                {
                    if (atoi(valor) > config_get_int_value((t_config *)archivo_config, "GRADO_MULTIPROGRAMACION"))
                    {
                        log_info(logger, "multigramacion mayor");
                        int diferencia = atoi(valor) - config_get_int_value((t_config *)archivo_config, "GRADO_MULTIPROGRAMACION");
                        for (int i = 0; i < diferencia; i++)
                            sem_post(&contador_grado_multiprogramacion); // no hace falta otro semaforo para ejecutar esto porque estos se atienden de forma atomica.
                    }
                    else if (atoi(valor) < config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION"))
                    {
                        log_info(logger, "multigramacion menor");
                        int diferencia = config_get_int_value((t_config *)archivo_config, "GRADO_MULTIPROGRAMACION") - atoi(valor);
                        for (int i = 0; i < diferencia; i++)
                            sem_wait(&contador_grado_multiprogramacion); // no hace falta otro semaforo para ejecutar esto porque estos se atienden de forma atomica.
                    }
                    config_set_value((t_config *)archivo_config, "GRADO_MULTIPROGRAMACION", valor);
                    config_save((t_config *)archivo_config);
                    printf("grado multiprogramacion cambiado a %s\n", valor);
                }
            }
            else if (strcmp(comando, "DETENER_PLANIFICACION") == 0)
            {
                // si la planificacion ya estaba detenida, no pierdo tiempo de procesamiento de procesos escribiendola de vuelta
                sem_wait(&mutex_planificacion_pausada);
                PLANIFICACION_PAUSADA = 1; // escritura
                sem_post(&mutex_planificacion_pausada);
                printf("detener planificacion\n");
            }
            else if (strcmp(comando, "INICIAR_PLANIFICACION") == 0)
            {
                // si la planificacion ya estaba corriendo, perdería tiempo de procesamiento si la excluyo para sobreescribirla con el mismo valor
                if(PLANIFICACION_PAUSADA != 0) { // como es lectura y sólo puede ocurrir otra lectura simultaneamente, no necesita semaforo (creo ja!)
                    sem_wait(&mutex_planificacion_pausada);
                    PLANIFICACION_PAUSADA = 0; // escritura
                    sem_post(&mutex_planificacion_pausada);
                }
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

t_pcb* buscar_pcb_por_pid(uint32_t pid_buscado){
    bool comparar_pid(void *elemento){
        t_pcb *pcb = (t_pcb *)elemento;
        //printf("tenemos el pid buscado %u y el pid actual %u\n", pid_buscado, pcb->pid);
        return pcb->pid == pid_buscado;
    }
    t_pcb* encontrado = (t_pcb*)list_find(pcb_list, comparar_pid);
    //if(encontrado) 
        //printf("encontramos el proceso %u\n", encontrado->pid);
    return encontrado; // si no funciona pasarlo como puntero a funcion
}

// definicion función hilo corto plazo
void *planificar_corto_plazo(void *archivo_config){
    while (1){
        sem_wait(&orden_planificacion); // solo se sigue la ejecución si ya largo plazo dejó al menos un proceso en READY
        sem_wait(&mutex_planificacion_pausada);
        if (!PLANIFICACION_PAUSADA){ // lectura
            sem_post(&mutex_planificacion_pausada);

            // 1. selecciona próximo proceso a ejecutar
            algoritmo_planificacion(); // ejecuta algorimo de planificacion corto plazo según valor del config
            log_info(logger, "estas en corto plazoo");
            sem_wait(&listo_proceso_en_running);

            // 2. se manda el proceso seleccionado a CPU a través del puerto dispatch
            enviar_proceso_a_cpu();

            // 3. se aguarda respuesta de CPU para tomar una decisión y continuar con la ejecución de procesos
            int mensaje_desalojo = recibir_operacion(conexion_cpu_dispatch); // bloqueante, hasta que CPU devuelva algo no continúa con la ejecución de otro proceso
            t_pcb* proceso_desalojado = mqueue_peek(monitor_RUNNING);
            sem_wait(&cambio_estado_desalojo);
            proceso_desalojado->desalojo = DESALOJADO;
            sem_post(&cambio_estado_desalojo);

            // Verifica nuevamente el estado de la planificación antes de procesar el desalojo
            sem_wait(&mutex_planificacion_pausada);
            // CONTINÚA SÓLO SI LA PLANIFICACION NO ESTÁ PAUSADA!!!
            while (PLANIFICACION_PAUSADA) {
                sem_post(&mutex_planificacion_pausada);
                sem_wait(&mutex_planificacion_pausada);
            }
            sem_post(&mutex_planificacion_pausada);

            if(proceso_desalojado->desalojo == PEDIDO_FINALIZACION){
                sem_wait(&cambio_estado_desalojo);
                mensaje_desalojo = FINALIZAR_PROCESO; // esto sólo se da cuando se lo finaliza desde consola 
                sem_post(&cambio_estado_desalojo);
            }

            switch (mensaje_desalojo){
            case EXIT_PROCESO:
            case FINALIZAR_PROCESO:
                log_info(logger, "se termino de ejecutar proceso");
                t_sbuffer *buffer_exit = cargar_buffer(conexion_cpu_dispatch);
                recupera_contexto_proceso(buffer_exit); // carga registros en el PCB del proceso en ejecución
                mqueue_push(monitor_EXIT, mqueue_pop(monitor_RUNNING));
                // TODO: log de finalización de proceso!!!! (según MENSAJE_DESALOJO)
                // el cambio de estado, la liberacion de memoria y el sem_post del grado de multiprogramación se encarga el módulo del plani largo plazo
                sem_post(&orden_proceso_exit);
                break;
            case WAIT_RECURSO:
                // 1. carga el buffer (1. paso en la mayoría de los mensajes de desalojo )
                // 2. lee únicamente (debería ser guardado en primer lugar) el nombre del recurso que pide la CPU utilizar
                // 3. valida que dicho recurso esté disponible: 
                    /*
                    A) si no está disponible => bloquea (RUNNING->BLOCKED_RECURSO), informa a CPU y ahora si ejecuta la función recuperar contexto del buffer que ya se encarga de actualizar en PCB
                    B) si está disponible => le resta una instancia al recurso y devuelve proceso a CPU (no hace falta seguir leyendo del buffer)
                    C) si no existe el nombre del recurso => recupera contexto del buffer actualizando PCB, (RUNNING->EXIT) ... mismo pasos que proceso EXIT
                    */
                // 4. libera buffer y otros recursos de memoria dinámica!!!!
                break;

            case SIGNAL_RECURSO:
                break;

            case FIN_QUANTUM:
                log_desalojo_fin_de_quantum(logger, proceso_desalojado->pid);
                t_sbuffer *buffer_desalojo = cargar_buffer(conexion_cpu_dispatch);
                recupera_contexto_proceso(buffer_desalojo);
                proceso_desalojado->estado = READY;
                mqueue_push(monitor_READY, mqueue_pop(monitor_RUNNING));
                log_cambio_estado_proceso(logger, proceso_desalojado->pid, "RUNNING", "READY");
                // sem_post(&contador_grado_multiprogramacion); SOLO se libera cuando proceso entra a EXIT ¿o cuando proceso se bloquea?
                break;
            // (...)
            }
            sem_wait(&cambio_estado_desalojo);
            proceso_desalojado->desalojo = SIN_DESALOJAR; // lo vuelvo a su estado inicial por si vuelve a READY y pasa a bloqueado y luego a READY nuevamente
            sem_post(&cambio_estado_desalojo);
            /*
            A. PUEDE PASAR QUE NECESITES BLOQUEAR EL PROCESO (recurso o I/O no disponible)=> ver si se tiene que delegar en otro hilo que haga el mediano plazo
            B. PUEDE PASAR QUE FINALICE EL PROCESO => mandarlo a cola EXIT
            // TODO: para casos A Y B: sem_post(&contador_grado_multiprogramacion); // libera un espacio en READY --> así deja de estar en memoria y continúa con la ejecución de otros procesos
            C. PUEDE PASAR QUE SE DESALOJE POR FIN DE QUANTUM: en dicho caso, se vuelve a pasar el proceso al FIN de la cola READY
            */
        }
        else{
            sem_post(&mutex_planificacion_pausada);
            sem_post(&orden_planificacion); // así vuelve a cargar en espera el proceso que NO se ejecutó por estar detenida la planificación
        }
    }
}

// definicion funcion hilo largo plazo
void *planificar_largo_plazo(void *archivo_config)
{
    pthread_t thread_NEW_READY, thread_ALL_EXIT;

    pthread_create(&thread_NEW_READY, NULL, planificar_new_to_ready, archivo_config);
    pthread_create(&thread_ALL_EXIT, NULL, planificar_all_to_exit, NULL);

    pthread_detach(thread_NEW_READY);
    pthread_detach(thread_NEW_READY);
    return NULL;
}

// -------------------- FIN FCS. DE HILOS KERNEL --------------------

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// ------------- INICIO FCS. HILO CONSOLA KERNEL -------------
void consola_interactiva(char* ruta_archivo){
    /*char* instruccion = malloc(50);
    char datoLeido;
    FILE* script = fopen(ruta_archivo, "rb+");
    if (script == NULL){
        log_error(logger, "No se encontró ningún archivo con el nombre indicado...");
    } else {
        while (!feof(script))
        {
            fread(&datoLeido, sizeof(char), sizeof(datoLeido), script);
            if(datoLeido == "\n"){
                printf("INSTRUCCION LEIDA %s", instruccion);
                consola_kernel(instruccion);
            }
        }
    }*/
}

void inicializar_registros(t_pcb* proceso) {
    proceso->registros.PC = 0;
    proceso->registros.AX = 0;
    proceso->registros.BX = 0;
    proceso->registros.CX = 0;
    proceso->registros.DX = 0;
    proceso->registros.EAX = 0;
    proceso->registros.EBX = 0;
    proceso->registros.ECX = 0;
    proceso->registros.EDX = 0;
    proceso->registros.SI = 0;
    proceso->registros.DI = 0;
}

// esto debería ir en memoria, y se ejecuta despues de verificar que el path existe.
void iniciar_proceso(char *path){

    t_pcb *proceso = malloc(sizeof(t_pcb));

    proceso->estado = NEW;
    proceso->quantum = config.quantum;
    proceso->program_counter = 0; // arranca en 0?
    proceso->pid = pid;
    inicializar_registros(proceso);
    strncpy(proceso->path, path, sizeof(proceso->path) - 1); // -1 para que no desborde
    proceso->path[sizeof(proceso->path) - 1] = '\0'; // en el último lugar se agrega caracter nulo
    for (int i = 0; i < 3; i++) { // TODO: por ahora consideramos 3 recursos, considerar la cantidad debe ser determinada en tiempos de ejecucion según la info del config kernel
        proceso->recursos[i] = 0; // Inicializar todos los recursos a 0
    }
    proceso->desalojo = SIN_DESALOJAR;

    mqueue_push(monitor_NEW, proceso);

    log_iniciar_proceso(logger, proceso->pid);

    if(list_is_empty(pcb_list))
        log_info(logger, "la lista esta vacia");

    list_add(pcb_list, proceso);
    pid++;
}

t_pcb* extraer_proceso(uint32_t pid_proceso, e_estado_proceso estado) {
    t_mqueue* monitor;

    // Determinar el monitor correspondiente según el estado
    switch (estado) {
        case NEW:
            monitor = monitor_NEW;
            break;
        case READY:
            monitor = monitor_READY;
            break;
        case RUNNING:
            monitor = monitor_RUNNING;
            break;
        case BLOCKED:
            monitor = monitor_BLOCKED;
            break;
        case EXIT:
            monitor = monitor_EXIT;
            break;
        default:
            return NULL; // Estado inválido
    }

    // Hago toda la zona crítica ya que pasa elementos de una cola a otra temporal hasta encontrar proceso y luego lo devuelve todo a su lugar
    sem_wait(&(monitor->mutex)); 

    // Función de condición para buscar el proceso por PID
    bool buscar_por_pid(void* data) {
        return ((t_pcb*)data)->pid == pid_proceso;
    }

    // busca y elimina procesode la cola
    t_pcb* proceso_encontrado = (t_pcb*)list_remove_by_condition(monitor->cola->elements, buscar_por_pid);

    sem_post(&(monitor->mutex)); 

    return proceso_encontrado;
}

// ------------- FIN FCS. HILO CONSOLA KERNEL -------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ------------- INICIO FUNCIONES PARA PLANI. CORTO PLAZO -------------
// ------ ALGORITMOS DE PLANIFICACION
fc_puntero obtener_algoritmo_planificacion(){
    if(strcmp(config.algoritmo_planificacion, "FIFO") == 0)
        return &algortimo_fifo;
    if(strcmp(config.algoritmo_planificacion, "RR") == 0)
        return &algoritmo_rr;
    if(strcmp(config.algoritmo_planificacion, "VRR") == 0){
        cola_READY_VRR = queue_create();
        return &algoritmo_vrr;
    }
    return NULL;
}

void sleep_half_second() {
    struct timespec tiempo;
    tiempo.tv_sec = 0;
    tiempo.tv_nsec = 200000000; // 500 millones de nanosegundos = 0.5 segundos
    nanosleep(&tiempo, NULL);
}

// cada algoritmo carga en (la cola?) running el prox. proceso a ejecutar, sacándolo de ready
void algortimo_fifo() {
    log_info(logger, "estas en fifo");

    t_pcb* primer_elemento = mqueue_pop(monitor_READY);
    primer_elemento->estado = RUNNING; 
    mqueue_push(monitor_RUNNING, primer_elemento);
    log_cambio_estado_proceso(logger, primer_elemento->pid, "READY", (char *)estado_proceso_strings[primer_elemento->estado]);
    sem_post(&listo_proceso_en_running); // manda señal al planificador corto plazo para que envie proceso a CPU a traves del puerto dispatch

}

// los algoritmos RR van a levantar un hilo que se encargará de, terminado el quantum, mandar a la cpu una interrupción para desalojar el proceso
void algoritmo_rr(){
    log_info(logger, "estas en rr");
    pthread_t hilo_RR;
    char* algoritmo = "RR";
    pthread_create(&hilo_RR, NULL, control_quantum, algoritmo);
    pthread_detach(hilo_RR);

}

void algoritmo_vrr(){
    log_info(logger, "estas en vrr");
    pthread_t hilo_RR; 
    char* algoritmo = "VRR";
    pthread_create(&hilo_RR, NULL, control_quantum, algoritmo);
    pthread_detach(hilo_RR);
}

void* control_quantum(void* tipo_algoritmo){
    int duracion_quantum = config.quantum;
    t_pcb* primer_elemento;
    int quantum_por_ciclo;
    // Pregunta si es VRR y si hay algún proceso en la cola de pendientes de VRR (con quantum menor).
    if(strcmp((char*)tipo_algoritmo, "VRR") == 0 && !queue_is_empty(cola_READY_VRR)){
        primer_elemento = queue_pop(cola_READY_VRR); // solo la modificamos desde aca!!
        quantum_por_ciclo = primer_elemento->quantum; // toma el quantum restante de su PCB
    } else {
        primer_elemento = mqueue_pop(monitor_READY);
        quantum_por_ciclo = duracion_quantum / 1000; 
    }

    primer_elemento->estado = RUNNING; 


    mqueue_push(monitor_RUNNING, primer_elemento); //TODO: se supone que se modifica de a uno y que no necesita SEMÁFOROS pero por el momento lo dejamos con el monitor (no debería ser una cola!!)
    log_cambio_estado_proceso(logger, primer_elemento->pid, "READY", "RUNNING");
    sem_post(&listo_proceso_en_running); // manda señal al planificador corto plazo para que envie proceso a CPU a traves del puerto dispatch

    // 1. verificas que el proceso siga en RUNNNIG dentro del QUANTUM establecido => de lo contrario, matas el hilo
    // 2. si se acabo el quantum, manda INTERRUPCION A CPU        
    for(int i = quantum_por_ciclo; i<=0; i--){ 
        //sleep(1);
        sleep_half_second();
        // TODO: EVALUAR SI ESTO DE ACA NO CORRESPONDERIA HACERLO CUANDO SE DEVUELVE EL PROCESO DE CPU con mensaje de desalojo distinto de quantum, asi no queda este hilo suelto dando lugar a errores de sincronizacion y la posibilidad de que mas de un hilo que ejecute control_quantum modifique las mismas variables simultaneamente
        if(primer_elemento->estado != RUNNING){ // si ya no esta en running (se desalojó el proceso por otro motivo que NO es FIN DE QUANTUM)
            if(strcmp(tipo_algoritmo, "VRR") == 0){
                /* A.VRR)
                1. cargar el quantum restante (i) en el PCB del proceso
                2. mandar el proceso a la cola READY_VRR
                */
            }
            pthread_exit(NULL); // solo sale del hilo actual => deja de ejecutar la funcion que lo llamo   
        }
    }
    log_debug(logger, "saliste del ciclo quantum");
    //primer_elemento->estado = READY; esto se maneja desde corto plazo luego de que se desaloja el proceso
    t_pic interrupt = {primer_elemento->pid, FIN_QUANTUM, 1};
    enviar_interrupcion_a_cpu(interrupt);
    // aca recien se manda INTERRUPCION A CPU!!!!! : cuando devuelva cpu a kernel, el planificador de mediano plazo (dentro de corot plazo) devuelve PRC a READY ante mensaje de desalojo FIN de QUANTUM
    
    // EVALUAR SI SE DEBE MANDAR EN CASO DE QUE, POR EJEMPLO, NO HAYA NADA EN LA COLA READY (RR) O EN LA COLA READY VRR (VRR): en ese caso, si mando INT, implicaría OVERHEAD para desp. volver a correr el mismo proceso que fue desalojado!!
    return NULL;
}

void enviar_interrupcion_a_cpu(t_pic interrupcion){
    t_sbuffer* buffer_interrupt = buffer_create(
        sizeof(uint32_t) // PID
        + sizeof(int) // COD_OP
        + sizeof(uint8_t) // BIT DE PRIORIDAD
    );

    buffer_add_uint32(buffer_interrupt, interrupcion.pid);
    buffer_add_int(buffer_interrupt, interrupcion.motivo_interrupcion);
    buffer_add_uint8(buffer_interrupt, interrupcion.bit_prioridad);

    cargar_paquete(conexion_cpu_interrupt, INTERRUPCION, buffer_interrupt); 
    log_info(logger, "envio interrupcion a cpu");
}

// ------- ENVIAR PCB POR DISPATCH
void enviar_proceso_a_cpu(){
    t_pcb* proceso = mqueue_peek(monitor_RUNNING); // lo tomo sin extraerlo
    // creo un buffer para pasar los datos a cpu
    t_sbuffer* buffer_dispatch = buffer_create(
        sizeof(uint32_t) * 9 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI + PID
        + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
    );

    buffer_add_uint32(buffer_dispatch, proceso->pid);
    buffer_add_registros(buffer_dispatch, &(proceso->registros));

    // una vez que tengo el buffer, lo envío a CPU!!
    cargar_paquete(conexion_cpu_dispatch, EJECUTAR_PROCESO, buffer_dispatch); 
    log_info(logger, "envio paquete a cpu");
}

// ------- RECUPERAR CONTEXTO ANTE DESALOJO DE PROCESO DE CPU
void recupera_contexto_proceso(t_sbuffer* buffer){
    t_pcb* proceso = mqueue_peek(monitor_RUNNING); 
    buffer_read_registros(buffer, &(proceso->registros));
    char *mensaje = (char *)malloc(128);
    sprintf(mensaje, "valor de PC: %u, valor de AX: %u, valor de BX: %u", proceso->registros.PC, proceso->registros.AX, proceso->registros.BX);
    log_info(logger, "%s", mensaje);
    free(mensaje);
}
// ------------- FIN FUNCIONES PARA PLANI. CORTO PLAZO -------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ------------- INICIO FCS. PLANI. LARGO PLAZO -------------
void *planificar_new_to_ready(void *archivo_config)
{
    while (1)
    {
        sem_wait(&mutex_planificacion_pausada);
        if (!PLANIFICACION_PAUSADA){ // lectura
            sem_post(&mutex_planificacion_pausada);
            if (!mqueue_is_empty(monitor_NEW)){
                sem_wait(&contador_grado_multiprogramacion);
                t_pcb* primer_elemento = mqueue_pop(monitor_NEW);
                primer_elemento->estado = READY;
                mqueue_push(monitor_READY, primer_elemento);
                log_cambio_estado_proceso(logger,primer_elemento->pid, "NEW", "READY");
                log_ingreso_ready(logger, monitor_READY);// esto seria el ingreso a ready que decis???
                // TODO: ingreso a READYs, 
                log_info(logger, "estas en largo plazoo");
                sem_post(&orden_planificacion);
            }
        }
        else
            sem_post(&mutex_planificacion_pausada);
    }
}

void *planificar_all_to_exit(void *args){

    /* 
    ----- PARA TODOS LOS OTROS CASOS, SEA DESDE CONSOLA O DESDE CORTO PLAZO, PREVIAMENTE SE AGREGÓ EL PRC DIRECTAMENTE A LA COLA EXIT, sacándolo primero de la cola desde donde se encuentra actualmente, haciendo actualizado ya su ctx en caso de corresponer Y SE MANEJA EL PASO A PASO DESDE ESTE HILO -----
    1. NEW: se libera memoria, se pasa a estado EXIT
    2. READY:  se libera memoria y recursos utilizados por el proceso (en caso de tenerlos), se aumenta un grado de multiprogramación, se pasa a estado EXIT
    3. RUNNING: se libera memoria, recursos utilizados por el proceso (en caso de tenerlos), se aumenta un grado de multiprogramacion, se pasa a estado EXIT
    4. BLOCKED: se lo saca de la cola desde donde esté bloqueado (analizar si ese puntero a cola se guarda en el pcb), se libera memoria y recursos, ¿se aumenta grado de multiprogramacion?, se pasa a estado EXIT
    5. EXIT: no se hace nada, ya está en EXIT...     

    luego, para todos los casos, se los saca de la cola EXIT
    */
    while (1){
        sem_wait(&orden_proceso_exit); // pasa sólo si hay algún proceso en cola EXIT
        sem_wait(&mutex_planificacion_pausada);
        if (!PLANIFICACION_PAUSADA){ // lectura
            sem_post(&mutex_planificacion_pausada);
            t_pcb* proceso_exit = mqueue_peek(monitor_EXIT);
            switch (proceso_exit->estado){
                case NEW:
                    proceso_exit->estado = EXIT;
                    liberar_proceso_en_memoria(proceso_exit->pid);
                    log_cambio_estado_proceso(logger, proceso_exit->pid, "NEW", "EXIT");
                    break;
                case READY:
                case RUNNING:
                case BLOCKED:
                    // ya desalojó correctamente desde donde correspondía (en caso de estado RUNNING)
                    op_code estado_actual = proceso_exit->estado;
                    liberar_proceso_en_memoria(proceso_exit->pid);
                    // TODO: libero recursos (en caso de tenerlos) => se podría agregar en el PCB un array de los recursos y otro para las interfaces que estaba utilizando hasta el momento, así se los libera desde acá
                    log_cambio_estado_proceso(logger, proceso_exit->pid, (char *)estado_proceso_strings[estado_actual], "EXIT");
                    proceso_exit->estado = EXIT;
                    sem_post(&contador_grado_multiprogramacion); // TODO: ANALIZAR SEGÚN LA DECISION QUE SE TOME SI CORRESPONDE LIBERAR MULTIPROGRAMACION PARA BLOCKED!
                    break;
                default:
                    break;
            }
            mqueue_pop(monitor_EXIT); // SACA PROCESO DE LA COLA EXIT
        } else{
            sem_post(&mutex_planificacion_pausada);
            sem_post(&orden_proceso_exit); // devuelvo el POST si la planificacion estaba desactivada y no se pudo continuar
        }
    }
    return NULL;
}

void liberar_memoria_proceso(t_pcb* proceso){
    if (proceso != NULL){
        // LIBERAR MEMORIA = mandar socket a MEMORIA para que libere el espacio reservado para dicho proceso en la MEMORIA
        //free(proceso->path); // no se asigna de forma dinámica

        // Si proceso->recursos es un array de punteros, se libera cada uno y luego el array
        /*for (int i = 0; i < proceso->recursos_count; i++) {
            free(proceso->recursos[i]);
        }*/ //ACA NECESITO SABER LA CANTIDAD DE RECURSOS
        // free(proceso->recursos); // no se asigna de forma dinámica

        // Finalmente, se libera el propio proceso
        //free(proceso); // no lo liberamos porque debe seguir guardado en EL PCB
    }   
}

// ------------- FIN FCS. PLANI. LARGO PLAZO -------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ------------- INICIO FUNCIONES PARA MULTIPLEXACION INTERFACES I/O -------------
void* atender_cliente(void* cliente){
	int cliente_recibido = *(int*) cliente;
	while(1){
		int cod_op = recibir_operacion(cliente_recibido); // bloqueante
		switch (cod_op)
		{
		case CONEXION:
			recibir_conexion(cliente_recibido); // TODO: acá KERNEL recibiría una conexion con una interfaz I/O 
			break;
		case PAQUETE:
			t_list* lista = recibir_paquete(cliente_recibido);
			log_info(logger, "Me llegaron los siguientes valores:\n");
			list_iterate(lista, (void*) iterator); //esto es un mapeo
			break;
		case -1:
			log_error(logger, "Cliente desconectado.");
			close(cliente_recibido); // cierro el socket accept del cliente
			free(cliente); // libero el malloc reservado para el cliente
			pthread_exit(NULL); // solo sale del hilo actual => deja de ejecutar la función atender_cliente que lo llamó
		default:
			log_warning(logger, "Operacion desconocida.");
			break;
		}
	}
}
// ------------- FIN FUNCIONES PARA MULTIPLEXACION INTERFACES I/O -------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ------------- INICIO FUNCIONES PARA MEMORIA -------------
void enviar_path_a_memoria(char* path){
    uint32_t longitud = (uint32_t)strlen(path);
    t_sbuffer* buffer_memoria = buffer_create(
        sizeof(char) * longitud
    );
    buffer_add_string(buffer_memoria, longitud, path);


    cargar_paquete(conexion_memoria, INICIAR_PROCESO, buffer_memoria); 
    log_info(logger, "envio path, para crear proceso, a memoria");
} //FUNCION iniciar_proceso VA EN MEMORIA TRAS RECIBIR ESTE COD_OP

void liberar_proceso_en_memoria(uint32_t pid_proceso){
    t_sbuffer* buffer_memoria = buffer_create(
        sizeof(uint32_t) //PID
    );
    buffer_add_uint32(buffer_memoria, pid_proceso);


    cargar_paquete(conexion_memoria, FINALIZAR_PROCESO, buffer_memoria); 
    log_info(logger, "envio pid, a liberar, a memoria");
} //FUNCION liberar_memoria_proceso VA EN MEMORIA TRAS RECIBIR ESTE COD_OP
// ------------- FIN FUNCIONES PARA MEMORIA -------------