#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <commons/temporal.h>
#include "logs.h" // logs incluye el main.h de kernel

/* TODO:
REVISAR BIEN QUE FUNCIONE OK COMANDOS DE MULTIPROGRAMACION Y DE INICIAR/DETENER PLANIFICACION (este commit está listo para probar eso)
*/

// ---------- CONSTANTES ---------- //
const char *estado_proceso_strings[] = {
    "NEW", 
    "READY", 
    "RUNNING", 
    "BLOCKED", 
    "EXIT"
};

// ---------- CONEXIONES - SOCKETS ---------- //
int conexion_cpu_dispatch, conexion_memoria, conexion_cpu_interrupt;

// ---------- GENERALES ---------- //
config_struct config;
t_list *pcb_list;
uint32_t pid; // PID: contador para determinar el PID de cada proceso creado

// ---------- RR + VRR ---------- //
fc_puntero algoritmo_planificacion;
t_mqueue *monitor_READY_VRR;
pthread_t hilo_quantum; // hilo para el control de QUANTUM en caso de RR O VRR
useconds_t quantum_en_microsegundos; // donde se guarda el quantum a controlar en caso de RR o VRR
int64_t  milisegundos_transcurridos;
uint8_t corresponde_quantum;
uint8_t corresponde_timer_vrr;
t_temporal* timer_quantum; // para VRR: contador en milisegundos para determinar el quantum restante!!!

// ---------- FLAGS ---------- //
uint8_t PLANIFICACION_PAUSADA; // 0: largo plazo NEW; 1: corto plazo + desalojo 2: mediano plazo (blocked->ready); 3: largo plazo EXIT

// ---------- RECURSOS - BLOQUEO ---------- //
int cantidad_recursos; // cantidad total de recursos grabados en el archivo de configuracion
char** instancias_recursos; // instancias disponibles por recurso
t_mqueue** cola_recursos_bloqueados; // para cada recurso del archivo configuración se va a cargar un monitor en este array de monitores, donde se encolarán los procesos que fueron bloqueados por cada recurso!
sem_t mutex_instancias_recursos; // mutex para las instancias dde cada recurso (se pueedde consultar/modificar simultáneamente desde largo plazo EXIT como desdde corto plazo DESALOJO)

// ---------- SEMAFOROS ---------- //
sem_t mutex_planificacion_pausada[4], contador_grado_multiprogramacion, orden_planificacion_corto_plazo, orden_planificacion_largo_plazo, orden_proceso_exit, cambio_estado_desalojo;

// ---------- INTERFACES --------- //
t_list* interfaces_genericas;

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
    crear_monitores();
    cantidad_recursos = string_array_size(config.recursos);
    instancias_recursos = config_get_array_value(archivo_config, "INSTANCIAS");
    cola_recursos_bloqueados = malloc(cantidad_recursos * sizeof(t_mqueue*));
    if (cola_recursos_bloqueados == NULL) {
        log_error(logger, "NO se pudo asignar espacio para monitores de recursos bloqueados");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < cantidad_recursos; i++)
        cola_recursos_bloqueados[i] = mqueue_create();

    interfaces_genericas = list_create();
    quantum_en_microsegundos = config.quantum * 1000; // para RR

    // -- INICIALIZACION SEMAFOROS -- //
    for(int j = 0; j < 4; j++) sem_init(&mutex_planificacion_pausada[j], 0, 1);
    sem_init(&orden_planificacion_corto_plazo, 0, 0);
    sem_init(&orden_planificacion_largo_plazo, 0, 0);
    sem_init(&contador_grado_multiprogramacion, 0, config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION"));
    sem_init(&orden_proceso_exit, 0, 0);
    sem_init(&cambio_estado_desalojo, 0, 1);
    sem_init(&mutex_instancias_recursos, 0, 1); 

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
    if (pthread_create(&thread_kernel_servidor, NULL, servidor_escucha, &socket_servidor) != 0){
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
    // destruyo monitores para recursos bloqueados
    for (int i = 0; i < cantidad_recursos; i++) {
        if (cola_recursos_bloqueados[i] != NULL)
            mqueue_destroy(cola_recursos_bloqueados[i]);
    }
    free(cola_recursos_bloqueados);

    for(int j = 0; j < 4; j++) sem_destroy(&mutex_planificacion_pausada[j]);
    sem_destroy(&contador_grado_multiprogramacion);
    sem_destroy(&orden_planificacion_corto_plazo);
    sem_destroy(&orden_planificacion_largo_plazo);
    sem_destroy(&cambio_estado_desalojo);
    sem_destroy(&orden_proceso_exit);
    sem_destroy(&mutex_instancias_recursos);

    list_destroy(interfaces_genericas);
    list_destroy(pcb_list);

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
void cargar_config_struct_KERNEL(t_config *archivo_config){
    config.puerto_escucha = config_get_string_value(archivo_config, "PUERTO_ESCUCHA");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
    config.ip_cpu = config_get_string_value(archivo_config, "IP_CPU");
    config.puerto_cpu_dispatch = config_get_string_value(archivo_config, "PUERTO_CPU_DISPATCH");
    config.puerto_cpu_interrupt = config_get_string_value(archivo_config, "PUERTO_CPU_INTERRUPT");
    config.algoritmo_planificacion = config_get_string_value(archivo_config, "ALGORITMO_PLANIFICACION");
    config.recursos = config_get_array_value(archivo_config, "RECURSOS");
    config.quantum = config_get_int_value(archivo_config, "QUANTUM");
}
// -------------------- FIN FCS. DE CONFIGURACION --------------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// -------------------- INICIO FCS. DE HILOS KERNEL --------------------
// definicion funcion hilo consola
void *consola_kernel(void *archivo_config){
    char *leido;

    while (1){
        leido = readline("> ");

        // Verificar si se ingresó algo
        if (strlen(leido) == 0){
            free(leido);
            break;
        }
        interpretar_comando_kernel(leido, archivo_config);
    }
    return NULL;    
}

void interpretar_comando_kernel(char* leido, void *archivo_config)
{
    char **tokens = string_split(leido, " ");
    char *comando = tokens[0];
    if (comando != NULL)
    {
        if (strcmp(comando, "EJECUTAR_SCRIPT") == 0 && string_array_size(tokens) >= 2){
            char *path = tokens[1];
            if (strlen(path) != 0 && path != NULL){
                scripts_kernel(path, archivo_config);
                printf("path ingresado (ejecutar_script): %s\n", path);
            }
        } // INICIAR_PROCESO /carpetaProcesos/proceso1
        else if (strcmp(comando, "INICIAR_PROCESO") == 0 && string_array_size(tokens) >= 2){
            char *path = tokens[1];
            if (strlen(path) != 0 && path != NULL){  
                // EN REALIDAD KERNEL MANDA A CREAR AL PROCESO EN MEMORIA CUANDO TOMA UN GRADO DE MULTIPROGRAMACIÓN PARA DICHO PROCESO (agrego buffer en largo plazo)
                iniciar_proceso(path);
                // ver funcion para comprobar existencia de archivo en ruta relativa en MEMORIA ¿acá o durante ejecución? => revisar consigna
                printf("path ingresado (iniciar_proceso): %s\n", path);
            }
        }else if (strcmp(comando, "FINALIZAR_PROCESO") == 0 && string_array_size(tokens) >= 2){
                char *pid_char = tokens[1], *endptr;
                uint32_t valor_uint_pid = strtoul(pid_char, &endptr, 10);
                if (strlen(pid_char) != 0 && pid_char != NULL && valor_uint_pid > 0 && *endptr == '\0'){
                    printf("pid ingresado (finalizar_proceso): %s\n", pid_char);
                    
                    // 1. detengo la planificación: así los procesos no cambian su estado ni pasan de una cola a otra!
                    if(PLANIFICACION_PAUSADA == 0){
                        log_debug(logger, "entro aca a detener");
                        for(int j = 0; j < 4; j++) sem_wait(&mutex_planificacion_pausada[j]); // toma uno a uno los waits sólo si están liberados!
                    }

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
                                
                                log_debug(logger, "mandaste interrupcion con FINALIZAR_PROCESO");
                            } else if (proceso_buscado->desalojo == DESALOJADO) {
                                // cuando el proceso fue desalojado de CPU pero todavía no se trató su motivo de desalojo
                                sem_wait(&cambio_estado_desalojo);
                                proceso_buscado->desalojo = PEDIDO_FINALIZACION; 
                                sem_post(&cambio_estado_desalojo);

                                log_debug(logger, "PEDIDO_FINALIZACION");
                                // la idea es que desde corto plazo sólo se procese su motivo de desalojo cuando no haya un pedido de finalizacion! y si hay un pedido de finalización, se mande el proceso a EXIT!
                            }
                                // en el caso de RUNNING, el proceso de desalojo (recibir el código de operación) se maneja desde CORTO PLAZO!
                        } else { 
                            // para cualquier otro estado: se lo saca de su cola actual y se lo pasa a EXIT desde acá!
                            t_pcb* proceso_encontrado = extraer_proceso(proceso_buscado); // lo saco de su cola actual
                            mqueue_push(monitor_EXIT, proceso_encontrado); // lo agrego a EXIT
                            log_finaliza_proceso(logger, proceso_encontrado->pid, "INTERRUPTED_BY_USER");
                            sem_post(&orden_proceso_exit);
                        }
                        }
                    } else
                        printf("pid no existente\n");

                    // 4. inicio la planificacion
                    if(PLANIFICACION_PAUSADA == 0){
                        for(int j = 0; j < 4; j++) sem_post(&mutex_planificacion_pausada[j]); // libero los waits del resto de los hilos solo si antes los tome!
                    }
            
            }
        }
        else if (strcmp(comando, "MULTIPROGRAMACION") == 0 && string_array_size(tokens) >= 2){
            char *valor = tokens[1];
            if (strlen(valor) != 0 && valor != NULL && atoi(valor) > 0){
                if (atoi(valor) > config_get_int_value((t_config *)archivo_config, "GRADO_MULTIPROGRAMACION")){
                    log_debug(logger, "multigramacion mayor");
                    int diferencia = atoi(valor) - config_get_int_value((t_config *)archivo_config, "GRADO_MULTIPROGRAMACION");
                    for (int i = 0; i < diferencia; i++)
                        sem_post(&contador_grado_multiprogramacion); // no hace falta otro semaforo para ejecutar esto porque estos se atienden de forma atomica.
                }
                else if (atoi(valor) < config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION")){
                    log_debug(logger, "multigramacion menor");
                    int diferencia = config_get_int_value((t_config *)archivo_config, "GRADO_MULTIPROGRAMACION") - atoi(valor);
                    for (int i = 0; i < diferencia; i++)
                        sem_wait(&contador_grado_multiprogramacion); // no hace falta otro semaforo para ejecutar esto porque estos se atienden de forma atomica.
                }
                config_set_value((t_config *)archivo_config, "GRADO_MULTIPROGRAMACION", valor);
                config_save((t_config *)archivo_config);
                printf("grado multiprogramacion cambiado a %s\n", valor);
            }
        }
        else if (strcmp(comando, "DETENER_PLANIFICACION") == 0){
            if (PLANIFICACION_PAUSADA == 0){
                for(int j = 0; j < 4; j++) sem_wait(&mutex_planificacion_pausada[j]); // toma uno a uno los waits sólo si están liberados!
                PLANIFICACION_PAUSADA = 1; // escritura
            }
            printf("detener planificacion\n");
        }
        else if (strcmp(comando, "INICIAR_PLANIFICACION") == 0){
            if(PLANIFICACION_PAUSADA == 1) { 
                for(int j = 0; j < 4; j++) sem_post(&mutex_planificacion_pausada[j]); // libero uno a uno los waits de otros hilos para continuar con la planificacion
                PLANIFICACION_PAUSADA = 0; // escritura
            }
            printf("iniciar planificacion\n");
        }
        else if (strcmp(comando, "PROCESO_ESTADO") == 0){
            // estados_procesos()
            printf("estados de los procesos\n");
        }
    }
    string_array_destroy(tokens);
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
        sem_wait(&orden_planificacion_corto_plazo); // solo se sigue la ejecución si ya largo plazo dejó al menos un proceso en READY o se desbloqueó algún proceso!
        sem_wait(&mutex_planificacion_pausada[1]); // continúa si no fue tomado el MUTEX desde DETENER_PLANIFICACION / FINALIZAR_PROCESO => no está pausada la planificación

        // 1. selecciona próximo proceso a ejecutar
        algoritmo_planificacion(); // ejecuta algorimo de planificacion corto plazo según valor del config
        sem_post(&mutex_planificacion_pausada[1]); // libero luego de hacer el cambio entre colas! 

        // 2. se manda el proceso seleccionado a CPU a través del puerto dispatch
        enviar_proceso_a_cpu(); // send()

        // 3. control QUANTUM una vez que el proceso está en RUNNING!!! (sólo para RR o VRR)
        if (corresponde_quantum == 1){ 
            t_pcb* proceso_en_running = mqueue_peek(monitor_RUNNING);
            if(corresponde_timer_vrr == 1 ) {
                timer_quantum = temporal_create(); // comienza a correr el cronómetro!! SÓLO PARA VRR
                log_debug(logger, "estas en quantum vrr creando el timer");
            }
            pthread_create(&hilo_quantum, NULL, control_quantum, &(proceso_en_running->pid));
            pthread_detach(hilo_quantum);  
        }

        // 4. se aguarda respuesta de CPU para tomar una decisión y continuar con la ejecución de procesos
        recibir_proceso_desalojado(); // recv()
    }
}

// definicion funcion hilo largo plazo
void *planificar_largo_plazo(void *archivo_config){
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
void scripts_kernel(char* ruta_archivo, void* archivo_config){
    char* instruccion = malloc(50);
    char datoLeido;
    FILE* script = fopen(ruta_archivo, "rb+");
    if (script == NULL){
        log_error(logger, "No se encontró ningún archivo con el nombre indicado...");
    } else {
        while (!feof(script)){
            fread(&datoLeido, sizeof(char), sizeof(datoLeido), script);
            if(datoLeido == '\n'){
                printf("INSTRUCCION LEIDA %s", instruccion);
                interpretar_comando_kernel(instruccion, archivo_config);
            }
        }
    }
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

void iniciar_proceso(char *path){

    t_pcb *proceso = malloc(sizeof(t_pcb));

    proceso->estado = NEW;
    proceso->quantum = config.quantum; // quantum inicial en milisegundos
    proceso->program_counter = 0;
    proceso->pid = pid;
    proceso->desalojo = SIN_DESALOJAR;
    inicializar_registros(proceso);
    strncpy(proceso->path, path, sizeof(proceso->path) - 1); // -1 para que no desborde
    proceso->path[sizeof(proceso->path) - 1] = '\0'; // en el último lugar se agrega caracter nulo

    proceso->recursos = malloc(cantidad_recursos * sizeof(int)); 
    for (int i = 0; i < cantidad_recursos; i++) {
        proceso->recursos[i] = 0; // inicializamos todas en cero 
    }

    mqueue_push(monitor_NEW, proceso);

    log_iniciar_proceso(logger, proceso->pid);

    list_add(pcb_list, proceso);
    pid++;
    sem_post(&orden_planificacion_largo_plazo);
}

t_pcb* extraer_proceso(t_pcb* proceso) {
    sem_t semaforo_mutex; 
    t_queue* cola;

    // Determinar el monitor correspondiente según el estado
    switch (proceso->estado) {
        case NEW:
            semaforo_mutex = monitor_NEW->mutex;
            cola = monitor_NEW->cola;
            break;
        case READY:
            // si estamos en VRR puede que el proceso esté en READY+ => compruebo esto chequeando el valor del quantum
            if(corresponde_timer_vrr == 1 && proceso->quantum < config.quantum){
                semaforo_mutex = monitor_READY_VRR->mutex;
                cola = monitor_READY_VRR->cola;
            } else {
                semaforo_mutex = monitor_READY->mutex;
                cola = monitor_READY->cola;
            }
            break;
        case BLOCKED:
            // toma de la cola que corresponda dentro de las colas de bloqueos
            cola = proceso->cola_bloqueado->cola;
            semaforo_mutex = proceso->cola_bloqueado->mutex;
            break;
        default:
            return NULL; // Estado inválido
    }

    // Hago toda la zona crítica ya que pasa elementos de una cola a otra temporal hasta encontrar proceso y luego lo devuelve todo a su lugar
    sem_wait(&semaforo_mutex); 

    // Función de condición para buscar el proceso por PID
    bool buscar_por_pid(void* data) {
        return ((t_pcb*)data)->pid == proceso->pid;
    };

    // busca y elimina procesode la cola
    t_pcb* proceso_encontrado = (t_pcb*)list_remove_by_condition(cola->elements, buscar_por_pid);

    sem_post(&semaforo_mutex); 

    return proceso_encontrado;
}

// ------------- FIN FCS. HILO CONSOLA KERNEL -------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ------------- INICIO FUNCIONES PARA PLANI. CORTO PLAZO -------------
// ------ ALGORITMOS DE PLANIFICACION
fc_puntero obtener_algoritmo_planificacion(){
    if(strcmp(config.algoritmo_planificacion, "FIFO") == 0){
        corresponde_quantum = 0;
        corresponde_timer_vrr = 0;
        return &algoritmo_fifo_rr;
    }
    if(strcmp(config.algoritmo_planificacion, "RR") == 0){
        corresponde_quantum = 1;
        corresponde_timer_vrr = 0;
        return &algoritmo_fifo_rr;
    }
    if(strcmp(config.algoritmo_planificacion, "VRR") == 0){
        monitor_READY_VRR = mqueue_create();
        corresponde_quantum = 1;
        corresponde_timer_vrr = 1;
        return &algoritmo_vrr;
    }
    return NULL;
}

// tanto FIFO como RR sacan el primer proceso en READY y lo mandan a RUNNING
void algoritmo_fifo_rr() {
    log_debug(logger, "estas en FIFO/RR");
    t_pcb* primer_elemento = mqueue_pop(monitor_READY);
    primer_elemento->estado = RUNNING; 
    mqueue_push(monitor_RUNNING, primer_elemento);
    log_cambio_estado_proceso(logger, primer_elemento->pid, "READY", "RUNNING");
}

// en el caso de VRR, verifica si hay un proceso con MAYOR PRIORIDAD (aquellos que fueron desalojados por IO) y manda esos procesos a ejecutar
void algoritmo_vrr(){
    log_debug(logger, "estas en VRR");
    t_pcb* primer_elemento;
    if(!mqueue_is_empty(monitor_READY_VRR)){
        primer_elemento = mqueue_pop(monitor_READY_VRR); // TODO: evaluar semáforo teniendo en cuenta donde se carga este monitor!!!
        quantum_en_microsegundos = primer_elemento->quantum * 1000; // toma el quantum restante de su PCB
    } else {
        primer_elemento = mqueue_pop(monitor_READY);
        primer_elemento->quantum = config.quantum; // me aseguro que si está en READY COMUN siempre parta del quantum del sistema!! (por si desalojó desp de volver de instruccion IO por recurso no disponible, por ejemplo)
        quantum_en_microsegundos = config.quantum * 1000; // toma el quantum del sistema como RR
    }
    primer_elemento->estado = RUNNING; 
    mqueue_push(monitor_RUNNING, primer_elemento);
    log_cambio_estado_proceso(logger, primer_elemento->pid, "READY", "RUNNING");
}

void* control_quantum(void* args){
    // 1. comienza a correr el quantum!
    log_debug(logger, "cuenta los siguientes microsegundos: %u", quantum_en_microsegundos);
    usleep(quantum_en_microsegundos);
    // 2. si se acabo el quantum, manda INTERRUPCION A CPU
    t_pic interrupt = {*(uint32_t*) args, FIN_QUANTUM, 1};
    enviar_interrupcion_a_cpu(interrupt);
    return NULL;
}

void enviar_interrupcion_a_cpu(t_pic interrupcion){
    // 1. asigno espacio en memoria dinámica => pasar por parámetro de buffer_create el tamaño total 
    t_sbuffer* buffer_interrupt = buffer_create(
        sizeof(uint32_t) // PID
        + sizeof(int) // COD_OP (enum)
        + sizeof(uint8_t) // BIT DE PRIORIDAD
    );

    buffer_add_uint32(buffer_interrupt, interrupcion.pid);
    buffer_add_int(buffer_interrupt, interrupcion.motivo_interrupcion);
    buffer_add_uint8(buffer_interrupt, interrupcion.bit_prioridad);

    cargar_paquete(conexion_cpu_interrupt, INTERRUPCION, buffer_interrupt); 
    log_debug(logger, "envio interrupcion para pid %u a cpu", interrupcion.pid);
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
    log_debug(logger, "envio paquete a cpu");
}

// -------- RECIBIR PROCESO DESALOJADO Y MANEJAR SU MOTIVOS DE DESALOJO
void recibir_proceso_desalojado(){
    // 1. Espera mensaje de desalojo desde CPU DISPATCH
    int mensaje_desalojo = recibir_operacion(conexion_cpu_dispatch); // bloqueante, hasta que CPU devuelva algo no continúa con la ejecución de otro proceso
    
    //  apenas desaloja cargo ms transcurridos para VRR    
    if(corresponde_timer_vrr == 1) {
        milisegundos_transcurridos = temporal_gettime(timer_quantum); 
        log_debug(logger, "ms transcurridos: %ld", milisegundos_transcurridos);
    }

    // 2. Carga buffer y consulta si es instrucción IO o no (0: no; 1: si)
    t_sbuffer *buffer_desalojo = cargar_buffer(conexion_cpu_dispatch);

    // 3. Carga motivo de desalojo!
    t_pcb* proceso_desalojado = mqueue_peek(monitor_RUNNING);
    sem_wait(&cambio_estado_desalojo);
    proceso_desalojado->desalojo = DESALOJADO;
    sem_post(&cambio_estado_desalojo);

    // Verifica nuevamente el estado de la planificación antes de procesar el desalojo
    sem_wait(&mutex_planificacion_pausada[1]); // continua si no está pausada la planificación!

    if(proceso_desalojado->desalojo == PEDIDO_FINALIZACION){
        sem_wait(&cambio_estado_desalojo);
        mensaje_desalojo = FINALIZAR_PROCESO; // esto sólo se da cuando se lo finaliza desde consola 
        sem_post(&cambio_estado_desalojo);
    }

    switch (mensaje_desalojo){
        case EXIT_PROCESO:
        case FINALIZAR_PROCESO:
            control_quantum_desalojo();
            log_debug(logger, "se termino de ejecutar proceso");
            recupera_contexto_proceso(buffer_desalojo); // carga registros en el PCB del proceso en ejecución
            mqueue_push(monitor_EXIT, mqueue_pop(monitor_RUNNING));
            sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!
            if(mensaje_desalojo == EXIT_PROCESO)
                log_finaliza_proceso(logger, proceso_desalojado->pid, "SUCCESS");
            else 
                log_finaliza_proceso(logger, proceso_desalojado->pid, "INTERRUPTED_BY_USER");
            // el cambio de estado, la liberacion de memoria y el sem_post del grado de multiprogramación se encarga el módulo del plani largo plazo
            sem_post(&orden_proceso_exit);
        break;


        // WAIT RA 
        case WAIT_RECURSO:
        case SIGNAL_RECURSO:

            // 1. lee únicamente el nombre del recurso que pide utilizar la CPU 
            uint32_t length;
            char* recurso_solicitado = buffer_read_string(buffer_desalojo, &length);
            recurso_solicitado[strcspn(recurso_solicitado, "\n")] = '\0'; // CORREGIR: DEBE SER UN PROBLEMA DESDE EL ENVÍO DEL BUFFER!
            
            log_debug(logger, "recibimos en WAIT/SIGNAL recurso %s", recurso_solicitado);

            // 2. validación y uso del recurso
            // 2.1 Lo busca en el array de RECURSOS
            int posicion_recurso = -1;
            for (int i = 0; i < cantidad_recursos; i++){
               if(strcmp(recurso_solicitado, config.recursos[i]) == 0){
                posicion_recurso = i;
                break;
               }
            }
            op_code respuesta_cpu;
            // A) NO existe el nombre del recurso: mandar a EXIT actualizando PCB
            if(posicion_recurso == -1){
                control_quantum_desalojo(); // SIEMPRE, si corresponde desalojar proceso por ERROR/BLOQUEO/EXIT/ => pausar el quantum y el timer
                recupera_contexto_proceso(buffer_desalojo); // actualizo PCB
                mqueue_push(monitor_EXIT, mqueue_pop(monitor_RUNNING)); // mando a EXIT
                sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!
                log_finaliza_proceso(logger, proceso_desalojado->pid, "INVALID_RESOURCE");
                sem_post(&orden_proceso_exit);
                respuesta_cpu = DESALOJAR;
            } else {
                // A PARTIR DE ACÁ COMPORTAMIENTO DEFINIDO POR LA OPERACIÓN RECIBIDA:
                switch (mensaje_desalojo){
                case WAIT_RECURSO:
                    sem_wait(&mutex_instancias_recursos);
                    if(atoi(instancias_recursos[posicion_recurso]) == 0){
                        // B) (WAIT) NO ESTA DISPONIBLE => se bloquea
                        control_quantum_desalojo(); // SIEMPRE, si corresponde desalojar proceso por ERROR/BLOQUEO/EXIT/ => pausar el quantum y el timer
                        recupera_contexto_proceso(buffer_desalojo); // actualizo PCB
                        proceso_desalojado->estado = BLOCKED;
                        mqueue_push(cola_recursos_bloqueados[posicion_recurso], mqueue_pop(monitor_RUNNING));
                        proceso_desalojado->cola_bloqueado = cola_recursos_bloqueados[posicion_recurso]; // TODO: SIMON HACER ALGO ASÍ PARA INTERFACES!!
                        sem_post(&mutex_instancias_recursos); // libero acá porque puede ocurrir un SIGNAL desde plani largo plazo EXIT
                        log_cambio_estado_proceso(logger, proceso_desalojado->pid, "RUNNING", "BLOCKED");
                        log_bloqueo_proceso(logger,proceso_desalojado->pid, recurso_solicitado);
                        sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!
                        respuesta_cpu = DESALOJAR;
                    } else {
                        // C) (WAIT) SI ESTÁ DISPONIBLE
                        sem_post(&mutex_planificacion_pausada[1]); // no hay cambio entre colas libero antes!
                        sprintf(instancias_recursos[posicion_recurso], "%d", (atoi(instancias_recursos[posicion_recurso]) - 1)); // resta una instancia a las instancias del recurso
                        sem_post(&mutex_instancias_recursos);
                        ++proceso_desalojado->recursos[posicion_recurso]; // agrega una instancia a los recursos del proceso en su PCB
                        respuesta_cpu = CONTINUAR;
                        buffer_destroy(buffer_desalojo);
                    }
                break;
                case SIGNAL_RECURSO:
                    // B) (SIGNAL) LIBEERA UNA INSTANCIA DEL RECURSO Y DESBLOQUEA PRIMER PROCESO BLOQUEADO PARA EL RECURSO (si es que lo hay)
                    sem_post(&mutex_planificacion_pausada[1]); // no hay cambio entre colas libero antes!
                    sem_wait(&mutex_instancias_recursos);
                    if(proceso_desalojado->recursos[posicion_recurso] > 0) --proceso_desalojado->recursos[posicion_recurso]; // podría hacer el signal sin hacer antes un wait!
                    // verificar si hay algún proceso en el monitor del recurso 
                    if(!mqueue_is_empty(cola_recursos_bloqueados[posicion_recurso])){
                        // sacar el primer proceso de la lista de los bloqueados y pasarlo a READY!!
                        sem_wait(&mutex_planificacion_pausada[2]); // control planificacion para mediano plazo!
                        t_pcb* proceso_desbloqueado = mqueue_pop(cola_recursos_bloqueados[posicion_recurso]);
                        ++proceso_desbloqueado->recursos[posicion_recurso];
                        sem_post(&mutex_instancias_recursos); // libero acá porque puede ocurrir un SIGNAL desde plani largo plazo EXIT
                        proceso_desbloqueado->estado = READY; // Siempre es READY (no READY+) porque VRR sólo prioriza I/O Bound (no procesos bloqueados por un recurso)
                        proceso_desbloqueado->cola_bloqueado = NULL;
                        mqueue_push(monitor_READY, proceso_desbloqueado); // suponemos que esos procesos todavía tienen un espacio reservado dentro del grado de multiprogramacion por lo tanto se pueden volver a agregar a READY sin problema
                        log_cambio_estado_proceso(logger, proceso_desbloqueado->pid, "BLOCKED", "READY");
                        sem_post(&mutex_planificacion_pausada[2]); // libera luego del cambio entre colas!
                        sem_post(&orden_planificacion_corto_plazo); // RECORDAR ESTO PARA AVISARLE AL PLANI DE CORTO PLAZO QUE ENTRÓ ALGO EN READY
                    } else {
                        sprintf(instancias_recursos[posicion_recurso], "%d", (atoi(instancias_recursos[posicion_recurso]) + 1)); // suma una instancia a las instancias del recurso
                        sem_post(&mutex_instancias_recursos);
                    }
                    respuesta_cpu = CONTINUAR;
                    buffer_destroy(buffer_desalojo);
                break;
                }
            }

            // 3. respuesta a CPU
            ssize_t bytes_enviados = send(conexion_cpu_dispatch, &respuesta_cpu, sizeof(respuesta_cpu), 0);
            if (bytes_enviados == -1) {
                perror("Error enviando el dato");
                exit(EXIT_FAILURE);
            }

            if(respuesta_cpu == CONTINUAR){
                sem_wait(&cambio_estado_desalojo);
                proceso_desalojado->desalojo = SIN_DESALOJAR; 
                sem_post(&cambio_estado_desalojo);
                recibir_proceso_desalojado(); //vuelve a ejecutar recibir_proceso_desalojado() para que kernel vuelva a esperar mensaje de desalojo del mismo proceso!
            }
            
        break;
        case FIN_QUANTUM:
            control_quantum_desalojo(); // SIEMPRE, si corresponde desalojar proceso por ERROR/BLOQUEO/EXIT/ => pausar el quantum y el timer
            recupera_contexto_proceso(buffer_desalojo);
            proceso_desalojado->estado = READY;
            proceso_desalojado->quantum = config.quantum; // reinicia el quantum en el valor inicial (para VRR)
            mqueue_push(monitor_READY, mqueue_pop(monitor_RUNNING));
            log_cambio_estado_proceso(logger, proceso_desalojado->pid, "RUNNING", "READY");
            sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!
            log_desalojo_fin_de_quantum(logger, proceso_desalojado->pid);
            sem_post(&orden_planificacion_corto_plazo);
            // sem_post(&contador_grado_multiprogramacion); SOLO se libera cuando proceso entra a EXIT ¿o cuando proceso se bloquea?
        break;
        
        case IO_GEN_SLEEP:
            control_quantum_desalojo(); // SIEMPRE, si corresponde desalojar proceso por ERROR/BLOQUEO/EXIT/ => pausar el quantum y el timer
            cargar_quantum_restante(proceso_desalojado); // al principio luego de cada instrucción IO
            uint32_t length_io;
            char* interfaz_solicitada = buffer_read_string(buffer_desalojo, &length_io);
            interfaz_solicitada[strcspn(interfaz_solicitada, "\n")] = '\0'; // CORREGIR: DEBE SER UN PROBLEMA DESDE EL ENVÍO DEL BUFFER!
            uint32_t tiempo = buffer_read_uint32(buffer_desalojo);
            recupera_contexto_proceso(buffer_desalojo); // guarda contexto en PCB
            
            char *mensaje_io = (char *)malloc(256);
            sprintf(mensaje_io, "recibimos en IN_GEN_SLEEP interfaz %s", interfaz_solicitada);
            log_debug(logger, "%s", mensaje_io);
            free(mensaje_io);

            // TODO: revisar si el nombre de la interfaz existe en la lista que corresponda => 1. si no existe EXIT, y sino avanzar
            /*
            mqueue_push(monitor_EXIT, mqueue_pop(monitor_RUNNING)); // mando a EXIT
            sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!
            log_finaliza_proceso(logger, proceso_desalojado->pid, "INVALID_INTERFACE");
            sem_post(&orden_proceso_exit);
            */
            // TODO: si el tipo de interfaz es de tipo GENERICA => 1. si no es: EXIT (lo mismo de arriba), desalojo proceso; 2. si la I/O esta disponible: mandas el buffer al socket de la interfaz y bloqueas (y agregar el proceso a cola FIFO proceso en uso por interfaz VER); 3. si la I/O no esta disponible, mandas el proceso a la cola de bloqueados de la interfaz y lo pones en estado blocked
            // sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!

            // Respuesta a CPU
            op_code respuesta_cpu_io = DESALOJAR;
            ssize_t bytes_enviados_io = send(conexion_cpu_dispatch, &respuesta_cpu_io, sizeof(respuesta_cpu_io), 0);
            if (bytes_enviados_io == -1) {
                perror("Error enviando el dato");
                exit(EXIT_FAILURE);
            }
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

// ejecutar al ppio. para los casos donde se desaloja el proceso en actual ejecución, cualquiera sea el motivo!
void control_quantum_desalojo(){ 
    if(corresponde_quantum == 1) pthread_cancel(hilo_quantum); // pausa hilo del quantum (para que deje de contar el quantum en caso de no haber desalojado por fin de quantum)
    if(corresponde_timer_vrr == 1) { // para el temporizador de vrr 
        temporal_stop(timer_quantum);
        temporal_destroy(timer_quantum);
        log_debug(logger, "timmer stop, quantum contado vrr: %ld", milisegundos_transcurridos );
    }
}

void cargar_quantum_restante(t_pcb* proceso_desalojado) {
    // -- cargo quantum restante si el algoritmo es VRR y si se desalojó por instruccion IO (si fuese desalojo por fin de recurso debería ir a READY comun)
    if(corresponde_timer_vrr == 1){
        log_debug(logger, "corresponde guardar quantum restante IO");
        if (milisegundos_transcurridos < proceso_desalojado->quantum ) 
            proceso_desalojado->quantum -= milisegundos_transcurridos;
        else 
            proceso_desalojado->quantum = config.quantum; // se consumió todo el QUANTUM RESTANTE: en estos casos luego de desalojar la IO debería volver a READY COMÚN
    }
}

// ------- RECUPERAR CONTEXTO ANTE DESALOJO DE PROCESO DE CPU
void recupera_contexto_proceso(t_sbuffer* buffer){
    t_pcb* proceso = mqueue_peek(monitor_RUNNING); 
    buffer_read_registros(buffer, &(proceso->registros));
    buffer_destroy(buffer);

    log_debug(logger, "PC: %u, AX: %u, BX: %u, CX: %u, DX: %u, EAX: %u, EBX: %u, ECX: %u, EDX: %u, SI: %u, DI: %u", proceso->registros.PC, proceso->registros.AX, proceso->registros.BX, proceso->registros.CX, proceso->registros.DX, proceso->registros.EAX, proceso->registros.EBX, proceso->registros.ECX, proceso->registros.EDX, proceso->registros.SI, proceso->registros.DI );
}
// ------------- FIN FUNCIONES PARA PLANI. CORTO PLAZO -------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ------------- INICIO FCS. PLANI. LARGO PLAZO -------------
void *planificar_new_to_ready(void *archivo_config)
{
    while (1){
        sem_wait(&orden_planificacion_largo_plazo); // solo cuando hayan procesos en NEW
        // 1. Tomo grado de multiprogramacion (TODO: devolver el grado de multiprogramacion si no se creo el proceso en memoria!!!)
        sem_wait(&contador_grado_multiprogramacion);
        sem_wait(&mutex_planificacion_pausada[0]);

        // 2. Envía orden a memoria para crear el proceso
        t_pcb* primer_elemento = mqueue_pop(monitor_NEW);
        t_sbuffer* buffer_proceso_a_memoria = buffer_create(
            sizeof(uint32_t) // pid 
            + (uint32_t)strlen(primer_elemento->path) + sizeof(uint32_t) // longitud del string más longitud en sí
        );

        buffer_add_uint32(buffer_proceso_a_memoria, primer_elemento->pid);
        buffer_add_string(buffer_proceso_a_memoria, (uint32_t)strlen(primer_elemento->path), primer_elemento->path);

        cargar_paquete(conexion_memoria, INICIAR_PROCESO, buffer_proceso_a_memoria); 
        // TODO: ACÁ MEMORIA DEBERÍA DEVOLVER ALGO PARA SABER SI SE CREÓ OK!!!!!!!!

        // 3. Envía a READY
        primer_elemento->estado = READY;
        mqueue_push(monitor_READY, primer_elemento);

        log_cambio_estado_proceso(logger,primer_elemento->pid, "NEW", "READY");
        log_ingreso_ready(logger, monitor_READY);
        sem_post(&mutex_planificacion_pausada[0]); // libera planificacion luego de mover entre colas

        // 4. Envia orden de procesamiento a corto plazo
        sem_post(&orden_planificacion_corto_plazo); 
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
        sem_wait(&mutex_planificacion_pausada[3]);
        t_pcb* proceso_exit = mqueue_peek(monitor_EXIT);
        switch (proceso_exit->estado){
            case NEW:
                proceso_exit->estado = EXIT;
                liberar_proceso_en_memoria(proceso_exit->pid);
                log_cambio_estado_proceso(logger, proceso_exit->pid, "NEW", "EXIT");
                sem_post(&mutex_planificacion_pausada[3]);
                break;
            case READY:
            case RUNNING:
            case BLOCKED:
                // ya desalojó correctamente desde donde correspondía (en caso de estado RUNNING)
                op_code estado_actual = proceso_exit->estado;
                liberar_recursos(proceso_exit); // puede asignar procesos a READY (BLOCKED -> READY) => va antes de liberar el grado de multiprogramación: (BLOCKED -> READY) > (NEW -> READY)
                liberar_proceso_en_memoria(proceso_exit->pid);// TODO: libero memoria => se le manda ORDEN para que libere los MARCOS y la tabla de páginas!
                log_cambio_estado_proceso(logger, proceso_exit->pid, (char *)estado_proceso_strings[estado_actual], "EXIT");
                sem_post(&mutex_planificacion_pausada[3]);
                proceso_exit->estado = EXIT;
                sem_post(&contador_grado_multiprogramacion); 
                break;
            default:
                break;
        }
        mqueue_pop(monitor_EXIT); // SACA PROCESO DE LA COLA EXIT
    }
    return NULL;
}

void liberar_recursos(t_pcb* proceso_exit){
    log_debug(logger, "entro a liberar recursos");
    // libero recursos (en caso de tenerlos) y desbloqueo procesos bloqueados para el recurso liberado 
    for (int i = 0; i < cantidad_recursos; i++){ // recorro por cada recurso existente en el sistema
        int instancias_utilizadas = proceso_exit->recursos[i];
        log_debug(logger, "estamos en el recurso %d con %d instancias", i, instancias_utilizadas);
        if(instancias_utilizadas != 0 ){ // si tengo al menos una instancia utilizada por el proceso actual para el recurso actual y aún no se la liberó
            for (int j = 0; j < instancias_utilizadas; j++){ // recorro según la cantidad de instancias que tenía el proceso asociadas por recurso
                // 1. Libero instancia desde instancias_recursos
                sem_wait(&mutex_instancias_recursos);

                // 2. Consulto si existe un proceso que se bloqueó para este recurso
                if(!mqueue_is_empty(cola_recursos_bloqueados[i])){
                    sem_wait(&mutex_planificacion_pausada[2]); // control planificacion para mediano plazo!
                    // 3. desbloqueo en dicho caso (todo lo demás se maneja dentro de la ejecución de dicho proceso)
                    t_pcb* proceso_desbloqueado = mqueue_pop(cola_recursos_bloqueados[i]);
                    log_debug(logger, "proceso bloqueado tenia %d instancias para dicho recurso, se le suma 1 ", proceso_desbloqueado->recursos[i]);
                    ++proceso_desbloqueado->recursos[i];
                    sem_post(&mutex_instancias_recursos);
                    proceso_desbloqueado->estado = READY; // Siempre es READY (no READY+) porque VRR sólo prioriza I/O Bound (no procesos bloqueados por un recurso)
                    proceso_desbloqueado->cola_bloqueado = NULL;
                    mqueue_push(monitor_READY, proceso_desbloqueado); // suponemos que esos procesos todavía tienen un espacio reservado dentro del grado de multiprogramacion por lo tanto se pueden volver a agregar a READY sin problema
                    log_cambio_estado_proceso(logger, proceso_desbloqueado->pid, "BLOCKED", "READY");
                    sem_post(&mutex_planificacion_pausada[2]); // control planificacion para mediano plazo!
                    sem_post(&orden_planificacion_corto_plazo); // RECORDAR ESTO PARA AVISARLE AL PLANI DE CORTO PLAZO QUE ENTRÓ ALGO EN READY
                } else {
                    log_debug(logger, "no hay procesos bloqueados se suma uno a las instancias ya disponibles %s", instancias_recursos[i]);
                    sprintf(instancias_recursos[i], "%d", (atoi(instancias_recursos[i]) + 1));
                    sem_post(&mutex_instancias_recursos);
                }

                // 3. Resto una instancia dentro de los recursos del proceso actual
                if(proceso_exit->recursos[i] > 0) --proceso_exit->recursos[i];
            }
        }
    }
    // en el caso de los recursos, luego de sumar las instancias que correspondan en instancias_recursps, hacer un free de los recursos del proceso! (ya que no lo vamos a usar más)
    free(proceso_exit->recursos);
    proceso_exit->recursos = NULL;
}

void liberar_memoria_proceso(t_pcb* proceso){
    if (proceso != NULL){
        // LIBERAR MEMORIA = mandar socket a MEMORIA para que libere el espacio reservado para dicho proceso en la MEMORIA
        //free(proceso->path); // no se asigna de forma dinámica

    }   
}

// ------------- FIN FCS. PLANI. LARGO PLAZO -------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ------------- INICIO FUNCIONES PARA MULTIPLEXACION INTERFACES I/O -------------
void* atender_cliente(void* cliente){
	int cliente_recibido = *(int*) cliente;
	while(1){
		int cod_op = recibir_operacion(cliente_recibido); // bloqueante
		switch (cod_op){
		case CONEXION:
			//recibir_conexion(cliente_recibido); // TODO: acá KERNEL recibiría una conexion con una interfaz I/O 
            t_sbuffer* buffer_conexion = cargar_buffer(cliente_recibido);
            uint32_t length;
            char* nombre_interfaz = buffer_read_string(buffer_conexion, &length);
            nombre_interfaz[strcspn(nombre_interfaz, "\n")] = '\0';

            // TODO: CARGAR STRUCT T_INTERFAZ
            t_interfaz* interfaz_conectada;
            interfaz_conectada->nombre_interfaz = nombre_interfaz; 
            interfaz_conectada->socket_interfaz = cliente_recibido;
            //(...)
            // según el tipo agregar al lista
            // AGREGAR T_INTERFAZ A LISTA DE INTERFACES (SE PUEDE CREAR UNA LISTA POR TIPO DE INTERFAZ)
            list_add(interfaces_genericas, interfaz_conectada);

			break;
        case IO_GEN_SLEEP:
            // TODO: la interfaz devuelve el proceso luego de ejecutar la instruccion IO_GEN_SLEEP y necesitas: desbloquear proceso actual y mandarlo a READY y además si hay un proceso bloqueado en la cola de esta interfaz, lo envías a la interfaz (hacer parecido lo mismo que hicimos en recibir_proceso_desalojado)

            // sólo si la interfaz no devolvió ERROR!!!! y una vez desbloqueado el proceso (se saca de la cola de bloqueados de la interfaz!)
            // la idea es que se ejecute la siguiente función que se encarga de mandar proceso a READY según algoritmo!
            // proceso_IO_to_READY(/*t_pcb* proceso*/);
            
        break;
        case RECIBI_INTERFAZ:   
            // cargar_buffer
            // buffer_read...
            // (...)
            break; 
		/*case PAQUETE:
			t_list* lista = recibir_paquete(cliente_recibido);
			log_debug(logger, "Me llegaron los siguientes valores:\n");
			list_iterate(lista, (void*) iterator); //esto es un mapeo
			break;
        */
		case -1:
			log_error(logger, "Cliente desconectado.");
            // (...)
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
void liberar_proceso_en_memoria(uint32_t pid_proceso){
    t_sbuffer* buffer_memoria = buffer_create(
        sizeof(uint32_t) //PID
    );
    buffer_add_uint32(buffer_memoria, pid_proceso);


    cargar_paquete(conexion_memoria, ELIMINAR_PROCESO, buffer_memoria); 
    log_debug(logger, "envio pid, a liberar, a memoria");
} //FUNCION liberar_memoria_proceso VA EN MEMORIA TRAS RECIBIR ESTE COD_OP
// ------------- FIN FUNCIONES PARA MEMORIA -------------