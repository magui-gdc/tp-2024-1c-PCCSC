#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <commons/temporal.h>
#include "logs.h" // logs incluye el main.h de kernel

// ---------- CONSTANTES ---------- //
const char *estado_proceso_strings[] = {
    "NEW", 
    "READY", 
    "EXEC", 
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

// ---------- INTERFACES DE IOS --------- //
t_list* interfaces_conectadas; // creo que conviene solo una lista de todas las interfaces para poder identificarlas más rápido en el sistema
sem_t mutex_interfaces_conectadas; // mutex para la lista de interfaces_conectadas

int main(int argc, char *argv[])
{
    // ------------ DECLARACION HILOS HIJOS PRINCIPALES ------------ //
    pthread_t thread_kernel_servidor, thread_kernel_consola, thread_planificador_corto_plazo, thread_planificador_largo_plazo;

    // ------------ ARCHIVOS CONFIGURACION + LOGGER ------------
    t_config *archivo_config = iniciar_config("kernelIO.config");
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

    quantum_en_microsegundos = config.quantum * 1000; // para RR
    interfaces_conectadas = list_create();

    // -- INICIALIZACION SEMAFOROS -- //
    for(int j = 0; j < 4; j++) sem_init(&mutex_planificacion_pausada[j], 0, 1);
    sem_init(&orden_planificacion_corto_plazo, 0, 0);
    sem_init(&orden_planificacion_largo_plazo, 0, 0);
    sem_init(&contador_grado_multiprogramacion, 0, config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION"));
    sem_init(&orden_proceso_exit, 0, 0);
    sem_init(&cambio_estado_desalojo, 0, 1);
    sem_init(&mutex_instancias_recursos, 0, 1); 
    sem_init(&mutex_interfaces_conectadas, 0, 1);

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
    if (pthread_create(&thread_kernel_consola, NULL, consola_kernel, archivo_config) != 0){
        log_error(logger, "No se ha podido crear el hilo para la consola kernel");
        exit(EXIT_FAILURE);
    }
    // hilo para planificacion a corto plazo (READY A EXEC)
    if (pthread_create(&thread_planificador_corto_plazo, NULL, planificar_corto_plazo, archivo_config) != 0){
        log_error(logger, "No se ha podido crear el hilo para el planificador corto plazo");
        exit(EXIT_FAILURE);
    }
    // hilo para planificacion a largo plazo (NEW A READY)
    if (pthread_create(&thread_planificador_largo_plazo, NULL, planificar_largo_plazo, archivo_config) != 0){
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
    sem_destroy(&mutex_interfaces_conectadas);

    list_destroy(interfaces_conectadas);
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
    config.path_scripts = config_get_string_value(archivo_config, "PATH_SCRIPTS");
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
        free(leido);

    }
    return NULL;    
}

void interpretar_comando_kernel(char* leido, void *archivo_config){
    char **tokens = string_split(leido, " ");
    char *comando = tokens[0];
    if (comando != NULL){
        if (strcmp(comando, "EJECUTAR_SCRIPT") == 0 && string_array_size(tokens) >= 2){
            char *path = tokens[1];
            if (strlen(path) != 0 && path != NULL){
                scripts_kernel(path, archivo_config);
            }
        } // INICIAR_PROCESO /carpetaProcesos/proceso1
        else if (strcmp(comando, "INICIAR_PROCESO") == 0 && string_array_size(tokens) >= 2){
            char *path = tokens[1];
            if (strlen(path) != 0 && path != NULL){  
                // EN REALIDAD KERNEL MANDA A CREAR AL PROCESO EN MEMORIA CUANDO TOMA UN GRADO DE MULTIPROGRAMACIÓN PARA DICHO PROCESO (agrego buffer en largo plazo)
                iniciar_proceso(path);
                // ver funcion para comprobar existencia de archivo en ruta relativa en MEMORIA ¿acá o durante ejecución? => revisar consigna
            }
        } else if (strcmp(comando, "FINALIZAR_PROCESO") == 0 && string_array_size(tokens) >= 2){
                char *pid_char = tokens[1], *endptr;
                uint32_t valor_uint_pid = strtoul(pid_char, &endptr, 10);
                if (strlen(pid_char) != 0 && pid_char != NULL && valor_uint_pid > 0 && *endptr == '\0'){
                    
                    // 1. detengo la planificación: así los procesos no cambian su estado ni pasan de una cola a otra!
                    if(PLANIFICACION_PAUSADA == 0){
                        log_debug(logger, "entro aca a detener");
                        for(int j = 0; j < 4; j++) sem_wait(&mutex_planificacion_pausada[j]); // toma uno a uno los waits sólo si están liberados!
                    }

                    // 2. pregunto por su estado
                    t_pcb* proceso_buscado = buscar_pcb_por_pid(valor_uint_pid);

                    if (proceso_buscado){

                    // 3. trabajo la solicitud de finalización según su estado
                    if(proceso_buscado->estado != EXIT){ // EXIT: Si ya está en EXIT no se debe procesar la solicitud
                        
                        // EXEC -> puede estar todavía en CPU o justo siendo desalojado (caso más borde pero bueno): 
                        if(proceso_buscado->estado == EXEC){ 
                            // cuando el proceso está en EXEC hay que avisarle a cpu que lo desaloje o a corto plazo que no procese su mensaje de desalojo, según donde esté
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
                                // en el caso de EXEC, el proceso de desalojo (recibir el código de operación) se maneja desde CORTO PLAZO!
                        } else { 
                            // para cualquier otro estado: se lo saca de su cola actual y se lo pasa a EXIT desde acá!
                            t_pcb* proceso_encontrado = extraer_proceso(proceso_buscado); // lo saco de su cola actual
                            mqueue_push(monitor_EXIT, proceso_encontrado); // lo agrego a EXIT
                            log_finaliza_proceso(logger, proceso_encontrado->pid, "INTERRUPTED_BY_USER");
                            sem_post(&orden_proceso_exit);
                        }
                        }
                    }

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
            }
        }
        else if (strcmp(comando, "DETENER_PLANIFICACION") == 0){
            if (PLANIFICACION_PAUSADA == 0){
                for(int j = 0; j < 4; j++) sem_wait(&mutex_planificacion_pausada[j]); // toma uno a uno los waits sólo si están liberados!
                PLANIFICACION_PAUSADA = 1; // escritura
            }
        }
        else if (strcmp(comando, "INICIAR_PLANIFICACION") == 0){
            if(PLANIFICACION_PAUSADA == 1) { 
                for(int j = 0; j < 4; j++) sem_post(&mutex_planificacion_pausada[j]); // libero uno a uno los waits de otros hilos para continuar con la planificacion
                PLANIFICACION_PAUSADA = 0; // escritura
            }
        }
        else if (strcmp(comando, "PROCESO_ESTADO") == 0){
            // 1. detengo la planificación: así los procesos no cambian su estado ni pasan de una cola a otra!
            if(PLANIFICACION_PAUSADA == 0)
                for(int j = 0; j < 4; j++) sem_wait(&mutex_planificacion_pausada[j]); // toma uno a uno los waits sólo si están liberados!

            t_list *listas_por_estado[5];
    
            // 2. Creo una lista por estado
            for (int i = 0; i < 5; i++) {
                listas_por_estado[i] = list_create();
            }

            // 3. Clasifico los procesos en las listas auxiliares según su estado
            for (int i = 0; i < list_size(pcb_list); i++) {
                t_pcb *proceso_estado = list_get(pcb_list, i);
                list_add(listas_por_estado[proceso_estado->estado], proceso_estado);
            }

            // 4. Imprimo pid por estado en el orden de NEW a EXIT
            for (int estado = NEW; estado <= EXIT; estado++) {
                printf("Procesos en estado %s:\n", estado_proceso_strings[estado]);
                for (int i = 0; i < list_size(listas_por_estado[estado]); i++) {
                    t_pcb *proceso_imprimir_pid = list_get(listas_por_estado[estado], i);
                    printf("PID: %u\n", proceso_imprimir_pid->pid);
                }
                printf("\n");
            }

            // 5. LIbero las listas auxiliares
            for (int i = 0; i < 5; i++) {
                list_destroy(listas_por_estado[i]);
            }
            
            
            // 6. inicio la planificacion
            if(PLANIFICACION_PAUSADA == 0)
                for(int j = 0; j < 4; j++) sem_post(&mutex_planificacion_pausada[j]); // libero los waits del resto de los hilos solo si antes los tome!

        }
    }
    string_array_destroy(tokens);
}

t_pcb* buscar_pcb_por_pid(uint32_t pid_buscado){
    bool comparar_pid(void *elemento){
        t_pcb *pcb = (t_pcb *)elemento;
        return pcb->pid == pid_buscado;
    }
    t_pcb* encontrado = (t_pcb*)list_find(pcb_list, comparar_pid);
    return encontrado; // si no funciona pasarlo como puntero a funcion
}

// definicion función hilo corto plazo
void *planificar_corto_plazo(void *archivo_config){
    while (1){
        sem_wait(&orden_planificacion_corto_plazo); // solo se sigue la ejecución si ya largo plazo dejó al menos un proceso en READY o se desbloqueó algún proceso!
        sem_wait(&mutex_planificacion_pausada[1]); // continúa si no fue tomado el MUTEX desde DETENER_PLANIFICACION / FINALIZAR_PROCESO => no está pausada la planificación

        // 1. selecciona próximo proceso a ejecutar
        int proceso_a_ready = algoritmo_planificacion(); // ejecuta algorimo de planificacion corto plazo según valor del config
        sem_post(&mutex_planificacion_pausada[1]); // libero luego de hacer el cambio entre colas! 
        
        if(proceso_a_ready){
            // 2. se manda el proceso seleccionado a CPU a través del puerto dispatch
            enviar_proceso_a_cpu(); // send()

            // 3. control QUANTUM una vez que el proceso está en EXEC!!! (sólo para RR o VRR)
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
        } // else: el proceso o alguno de los procesos que estaba en READY fue eliminado por orden del usuario
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
    char* instruccion = NULL;
    size_t len = 0;

    char* path_script = malloc(strlen(config.path_scripts) + strlen(ruta_archivo) + 1);
    strcpy(path_script, config.path_scripts);
    strcat(path_script, ruta_archivo);

    FILE* script = fopen(path_script, "r");
    if (script == NULL){
        log_error(logger, "No se encontró ningún SCRIPT con el nombre indicado...");
    } else {
        while (getline(&instruccion, &len, script) != -1){
            instruccion[strcspn(instruccion, "\n")] = '\0';
            interpretar_comando_kernel(instruccion, archivo_config);
            free(instruccion);
            instruccion = NULL;
        }
        fclose(script);
    }
    free(path_script);
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

    proceso->pid = pid;
    proceso->estado = NEW;
    inicializar_registros(proceso);
    proceso->quantum = config.quantum; // quantum inicial en milisegundos
    proceso->desalojo = SIN_DESALOJAR;
    strncpy(proceso->path, path, sizeof(proceso->path) - 1); // -1 para que no desborde
    proceso->path[sizeof(proceso->path) - 1] = '\0'; // en el último lugar se agrega caracter nulo
    proceso->recursos = malloc(cantidad_recursos * sizeof(int)); 
    for (int i = 0; i < cantidad_recursos; i++) {
        proceso->recursos[i] = 0; // inicializamos todas en cero 
    }
    proceso->cola_bloqueado = NULL;
    proceso->instruccion_io = NULL;

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

// tanto FIFO como RR sacan el primer proceso en READY y lo mandan a EXEC
int algoritmo_fifo_rr() {
    log_debug(logger, "estas en FIFO/RR");
    if(!mqueue_is_empty(monitor_READY)){
        t_pcb* primer_elemento = mqueue_pop(monitor_READY);
        primer_elemento->estado = EXEC; 
        mqueue_push(monitor_RUNNING, primer_elemento);
        log_cambio_estado_proceso(logger, primer_elemento->pid, "READY", "EXEC");
        return 1;
    } 
    return 0;
}

// en el caso de VRR, verifica si hay un proceso con MAYOR PRIORIDAD (aquellos que fueron desalojados por IO) y manda esos procesos a ejecutar
int algoritmo_vrr(){
    log_debug(logger, "estas en VRR");
    int proceso_a_ready = 0;
    t_pcb* primer_elemento;
    if(!mqueue_is_empty(monitor_READY_VRR)){
        primer_elemento = mqueue_pop(monitor_READY_VRR); 
        quantum_en_microsegundos = primer_elemento->quantum * 1000; // toma el quantum restante de su PCB
        log_debug(logger, "tomo quantum restante del proceso!! %u", primer_elemento->quantum);
        proceso_a_ready = 1;
    } else if(!mqueue_is_empty(monitor_READY)) {
        primer_elemento = mqueue_pop(monitor_READY);
        primer_elemento->quantum = config.quantum; // me aseguro que si está en READY COMUN siempre parta del quantum del sistema!! (por si desalojó desp de volver de instruccion IO por recurso no disponible, por ejemplo)
        quantum_en_microsegundos = config.quantum * 1000; // toma el quantum del sistema como RR
        proceso_a_ready = 1;
    }
    if(proceso_a_ready){
        primer_elemento->estado = EXEC; 
        mqueue_push(monitor_RUNNING, primer_elemento);
        log_cambio_estado_proceso(logger, primer_elemento->pid, "READY", "EXEC");
        return proceso_a_ready;
    } 
    return proceso_a_ready;
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
    if(corresponde_timer_vrr == 1)
        milisegundos_transcurridos = temporal_gettime(timer_quantum); 

    // 2. Carga buffer y consulta si es instrucción IO o no (0: no; 1: si)
    t_sbuffer *buffer_desalojo = cargar_buffer(conexion_cpu_dispatch);

    // 3. Carga motivo de desalojo!
    t_pcb* proceso_desalojado = mqueue_peek(monitor_RUNNING);
    sem_wait(&cambio_estado_desalojo);
    if(proceso_desalojado->desalojo == SIN_DESALOJAR) proceso_desalojado->desalojo = DESALOJADO;
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
        case OUT_OF_MEMORY:
            control_quantum_desalojo();
            log_debug(logger, "se termino de ejecutar proceso");
            recupera_contexto_proceso(buffer_desalojo); // carga registros en el PCB del proceso en ejecución
            mqueue_push(monitor_EXIT, mqueue_pop(monitor_RUNNING));
            sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!
            char motivo[25];
            strcpy(motivo, (mensaje_desalojo == EXIT_PROCESO) ? "SUCCESS" : ((mensaje_desalojo == OUT_OF_MEMORY) ? "OUT_OF_MEMORY" : "INTERRUPTED_BY_USER"));
            log_finaliza_proceso(logger, proceso_desalojado->pid, motivo);
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
                        proceso_desalojado->cola_bloqueado = cola_recursos_bloqueados[posicion_recurso]; 
                        sem_post(&mutex_instancias_recursos); // libero acá porque puede ocurrir un SIGNAL desde plani largo plazo EXIT
                        log_cambio_estado_proceso(logger, proceso_desalojado->pid, "EXEC", "BLOCKED");
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
                    sem_wait(&mutex_planificacion_pausada[2]); // control planificacion para mediano plazo!
                    if(!mqueue_is_empty(cola_recursos_bloqueados[posicion_recurso])){
                        // sacar el primer proceso de la lista de los bloqueados y pasarlo a READY!!
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
                        sem_post(&mutex_planificacion_pausada[2]); // libero planificacion
                        sprintf(instancias_recursos[posicion_recurso], "%d", (atoi(instancias_recursos[posicion_recurso]) + 1)); // suma una instancia a las instancias del recurso
                        sem_post(&mutex_instancias_recursos);
                    }
                    respuesta_cpu = CONTINUAR;
                    buffer_destroy(buffer_desalojo);
                break;
                }
            }

            free(recurso_solicitado);

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
            log_cambio_estado_proceso(logger, proceso_desalojado->pid, "EXEC", "READY");
            sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!
            log_desalojo_fin_de_quantum(logger, proceso_desalojado->pid);
            sem_post(&orden_planificacion_corto_plazo);
            // sem_post(&contador_grado_multiprogramacion); SOLO se libera cuando proceso entra a EXIT ¿o cuando proceso se bloquea?
        break;
        
        case IO_GEN_SLEEP:
        case IO_STDIN_READ:
        case IO_STDOUT_WRITE:
        case IO_FS_CREATE:
        case IO_FS_DELETE:
        case IO_FS_TRUNCATE:
        case IO_FS_WRITE:
        case IO_FS_READ:
            manejo_instruccion_io(mensaje_desalojo, buffer_desalojo, proceso_desalojado);
        break;
        // (...)
    }
    sem_wait(&cambio_estado_desalojo);
    proceso_desalojado->desalojo = SIN_DESALOJAR; // lo vuelvo a su estado inicial por si vuelve a READY o si pasa a bloqueado y luego a READY nuevamente
    sem_post(&cambio_estado_desalojo);
}

// ejecutar al ppio. para los casos donde se desaloja el proceso en actual ejecución, cualquiera sea el motivo!
void control_quantum_desalojo(){ 
    if(corresponde_quantum == 1) pthread_cancel(hilo_quantum); // pausa hilo del quantum (para que deje de contar el quantum en caso de no haber desalojado por fin de quantum)
    if(corresponde_timer_vrr == 1) { // para el temporizador de vrr 
        temporal_stop(timer_quantum);
        temporal_destroy(timer_quantum);
        log_debug(logger, "timer stop, quantum contado vrr: %ld", milisegundos_transcurridos );
    }
}

void cargar_quantum_restante(t_pcb* proceso_desalojado) {
    // -- cargo quantum restante si el algoritmo es VRR y si se desalojó por instruccion IO (si fuese desalojo por fin de recurso debería ir a READY comun)
    if(corresponde_timer_vrr == 1){
        if (milisegundos_transcurridos < proceso_desalojado->quantum){
            proceso_desalojado->quantum -= milisegundos_transcurridos;
            log_debug(logger, "corresponde guardar quantum restante IO, proceso %u quantum restante %u", proceso_desalojado->pid, proceso_desalojado->quantum);
        } 
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

void manejo_instruccion_io(int instruccion, t_sbuffer* buffer_desalojo, t_pcb* proceso_desalojado){
    // 1. Controles QUANTUM
    control_quantum_desalojo(); // SIEMPRE, si corresponde desalojar proceso por ERROR/BLOQUEO/EXIT/ => pausar el quantum y el timer
    cargar_quantum_restante(proceso_desalojado); // al principio luego de cada instrucción IO
    
    // 2. Identifico el tipo de interfaz que corresponde según la instrucción y tomo los datos del buffer
    char tipo_interfaz[11];
    uint32_t length_io;
    char* interfaz_solicitada = buffer_read_string(buffer_desalojo, &length_io); // en todas las INST de IO llega primero el nombre de la interfaz
    interfaz_solicitada[strcspn(interfaz_solicitada, "\n")] = '\0';

    t_sbuffer* buffer_instruccion_io; // aca voy cargando el buffer para enviar / reservar según si la interfaz esté disponible o no

    if(instruccion == IO_GEN_SLEEP){
        strcpy(tipo_interfaz, "GENERICA");
        buffer_instruccion_io = buffer_create(sizeof(uint32_t) * 2);
        buffer_add_uint32(buffer_instruccion_io, proceso_desalojado->pid);
        buffer_add_uint32(buffer_instruccion_io, buffer_read_uint32(buffer_desalojo)); // CANTIDAD UNIDADES DE TRABAJO
        recupera_contexto_proceso(buffer_desalojo); // guarda contexto en PCB y libera buffer

    } else if(instruccion == IO_STDIN_READ || instruccion == IO_STDOUT_WRITE){
        if(instruccion == IO_STDIN_READ) strcpy(tipo_interfaz, "STDIN"); else strcpy(tipo_interfaz, "STDOUT");
        buffer_read_registros(buffer_desalojo, &(proceso_desalojado->registros)); // actualiza contexto
        uint32_t nuevo_tamanio = buffer_desalojo->size - length_io -  sizeof(uint32_t) * 9 - sizeof(uint8_t) * 4; // le resto tamanio del nombre interfaz + registros 
        buffer_instruccion_io = buffer_create(nuevo_tamanio);
        buffer_read(buffer_desalojo, buffer_instruccion_io->stream, nuevo_tamanio); // el resto del buffer lo copia en el nuevo buffer  
        buffer_destroy(buffer_desalojo);

    } else if (instruccion == IO_FS_CREATE || instruccion == IO_FS_DELETE){
        strcpy(tipo_interfaz, "DIALFS");
        uint32_t length_file;
        char* nombre_file = buffer_read_string(buffer_desalojo, &length_file);
        recupera_contexto_proceso(buffer_desalojo);
        buffer_instruccion_io = buffer_create(length_file + sizeof(uint32_t) * 2);
        buffer_add_uint32(buffer_instruccion_io, proceso_desalojado->pid);
        buffer_add_string(buffer_instruccion_io, length_file, nombre_file); // NOMBRE ARCHIVO FILE SYSTEM
        free(nombre_file);

    } else if (instruccion == IO_FS_TRUNCATE){
        strcpy(tipo_interfaz, "DIALFS");
        uint32_t length_file;
        char* nombre_file = buffer_read_string(buffer_desalojo, &length_file);
        buffer_instruccion_io = buffer_create(length_file + sizeof(uint32_t) * 3);
        buffer_add_uint32(buffer_instruccion_io, proceso_desalojado->pid);
        buffer_add_string(buffer_instruccion_io, length_file, nombre_file); // NOMBRE ARCHIVO FILE SYSTEM
        buffer_add_uint32(buffer_instruccion_io, buffer_read_uint32(buffer_desalojo)); // REGISTRO TAMANIO
        recupera_contexto_proceso(buffer_desalojo);
        free(nombre_file);
        
    } else if (instruccion == IO_FS_WRITE || IO_FS_READ){
        strcpy(tipo_interfaz, "DIALFS");
        buffer_read_registros(buffer_desalojo, &(proceso_desalojado->registros)); // actualiza contexto
        uint32_t nuevo_tamanio = buffer_desalojo->size - length_io -  sizeof(uint32_t) * 9 - sizeof(uint8_t) * 4; // le resto tamanio del nombre interfaz + registros 
        buffer_instruccion_io = buffer_create(nuevo_tamanio);
        buffer_read(buffer_desalojo, buffer_instruccion_io->stream, nuevo_tamanio); // el resto del buffer lo copia en el nuevo buffer  
        buffer_destroy(buffer_desalojo);
    }

    log_debug(logger, "recibimos instruccion IO %d de tipo %s la interfaz %s.", instruccion, tipo_interfaz, interfaz_solicitada);
    
    // 3. Revisar si el nombre de la interfaz existe en la lista de interfaces conectadas => EXISTE: continua; NO EXISTE: mandar proceso a EXIT
    bool buscar_por_nombre(void* data) {
        return strcmp(((t_interfaz*)data)->nombre_interfaz, interfaz_solicitada) == 0;
    };

    sem_wait(&mutex_interfaces_conectadas);
    t_interfaz* interfaz = (t_interfaz*)list_find(interfaces_conectadas, buscar_por_nombre);
    sem_post(&mutex_interfaces_conectadas);

    free(interfaz_solicitada);

    if(!interfaz) { // la interfaz no existe o no está conectada
        mqueue_push(monitor_EXIT, mqueue_pop(monitor_RUNNING)); // mando a EXIT
        sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!
        buffer_destroy(buffer_instruccion_io); // al final NO se utiliza
        log_debug(logger, "estas en interfaz inexistente %u", proceso_desalojado->pid);
        log_finaliza_proceso(logger, proceso_desalojado->pid, "INVALID_INTERFACE");
        sem_post(&orden_proceso_exit);
    } else {
        if (strcmp(interfaz->tipo_interfaz, tipo_interfaz) == 0){
            // con este semáforo garantizamos que sólo se bloquee el hilo de la interfaz en cuestión y NO otross!
            sem_wait(&(interfaz->cola_bloqueados->mutex)); // bloqueo para que NO se libere este mismo proceso desde el hilo IO hasta que no tenga cargada su instrucción en caso de corresponder!!! y tmb para esperar el cambio de disponibilidad desde el otro hilo
            proceso_desalojado->estado = BLOCKED;
            queue_push(interfaz->cola_bloqueados->cola, mqueue_pop(monitor_RUNNING)); 
            proceso_desalojado->cola_bloqueado = interfaz->cola_bloqueados;
            log_cambio_estado_proceso(logger, proceso_desalojado->pid, "EXEC", "BLOCKED");
            sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!
            if(interfaz->disponibilidad == 0) { // interfaz disponible! => NO había otro proceso bloqueado para dicha interfaz
                interfaz->pid_ejecutando = proceso_desalojado->pid;
                cargar_paquete(interfaz->socket_interfaz, instruccion, buffer_instruccion_io); // ya libera el buffer
                interfaz->disponibilidad = 1; // interfaz ocupada!
                sem_post(&(interfaz->cola_bloqueados->mutex));
                log_debug(logger, "se bloquea proceso y se envia orden a interfaz disponible");
            } else { // interfaz no disponible
                // se carga en el paquete instruccion_io del proceso la instrucción a ejecutar cuando se libere dicha interfaz!
                t_spaquete* paquete_instruccion_io = malloc(sizeof(t_spaquete));
                paquete_instruccion_io->codigo_operacion = instruccion; 
                paquete_instruccion_io->buffer = buffer_instruccion_io; // se libera el buffer una vez que es enviado!!! 
                proceso_desalojado->instruccion_io = paquete_instruccion_io;
                sem_post(&(interfaz->cola_bloqueados->mutex));
                log_debug(logger, "se bloquea proceso y queda a espera de que se libere la interfaz");
                // LUEGO => se liberará desde ATENDER_CLIENTE cuando se libere la interfaz de la instruccion que esté ejecutando actualmente, cuando chequee en su lista de bloqueados!
            }
        } else { // la operación no corresponde con su tipo de interfaz 
            mqueue_push(monitor_EXIT, mqueue_pop(monitor_RUNNING)); // mando a EXIT
            sem_post(&mutex_planificacion_pausada[1]); // libera luego del cambio entre colas!
            buffer_destroy(buffer_instruccion_io); // al final NO se utiliza
            log_debug(logger, "lainterfaz no corresponde con su tipo %u", proceso_desalojado->pid);
            log_finaliza_proceso(logger, proceso_desalojado->pid, "INVALID_INTERFACE");
            sem_post(&orden_proceso_exit);
        }
    }
}
// ------------- FIN FUNCIONES PARA PLANI. CORTO PLAZO -------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// ------------- INICIO FCS. PLANI. LARGO PLAZO -------------
void *planificar_new_to_ready(void *archivo_config){
    while (1){
        sem_wait(&orden_planificacion_largo_plazo); // solo cuando hayan procesos en NEW
        // 1. Tomo grado de multiprogramacion 
        sem_wait(&contador_grado_multiprogramacion);
        sem_wait(&mutex_planificacion_pausada[0]);

        if(!mqueue_is_empty(monitor_NEW)){
            // 2. Envía orden a memoria para crear el proceso
            t_pcb* primer_elemento = mqueue_pop(monitor_NEW);
            t_sbuffer* buffer_proceso_a_memoria = buffer_create(
                sizeof(uint32_t) // pid 
                + (uint32_t)strlen(primer_elemento->path) + sizeof(uint32_t) // longitud del string más longitud en sí
            );

            buffer_add_uint32(buffer_proceso_a_memoria, primer_elemento->pid);
            buffer_add_string(buffer_proceso_a_memoria, (uint32_t)strlen(primer_elemento->path), primer_elemento->path);

            cargar_paquete(conexion_memoria, INICIAR_PROCESO, buffer_proceso_a_memoria); 
            recibir_operacion(conexion_memoria); // NO continúo hasta que memoria haya creado correctamente el proceso!

            // 3. Envía a READY
            primer_elemento->estado = READY;
            mqueue_push(monitor_READY, primer_elemento);

            log_cambio_estado_proceso(logger,primer_elemento->pid, "NEW", "READY");
            log_ingreso_ready(logger, monitor_READY);

            // 4. Envia orden de procesamiento a corto plazo
            sem_post(&orden_planificacion_corto_plazo); 
        } else { // el proceso o alguno de los que estaba en new se pudo haber mandado a finalizar
             sem_post(&contador_grado_multiprogramacion);
        }
        sem_post(&mutex_planificacion_pausada[0]); // libera planificacion luego de mover entre colas
    }
}

void *planificar_all_to_exit(void *args){

    /* 
    ----- PARA TODOS LOS OTROS CASOS, SEA DESDE CONSOLA O DESDE CORTO PLAZO, PREVIAMENTE SE AGREGÓ EL PRC DIRECTAMENTE A LA COLA EXIT, sacándolo primero de la cola desde donde se encuentra actualmente, haciendo actualizado ya su ctx en caso de corresponer Y SE MANEJA EL PASO A PASO DESDE ESTE HILO -----
    1. NEW: se libera memoria, se pasa a estado EXIT
    2. READY:  se libera memoria y recursos utilizados por el proceso (en caso de tenerlos), se aumenta un grado de multiprogramación, se pasa a estado EXIT
    3. EXEC: se libera memoria, recursos utilizados por el proceso (en caso de tenerlos), se aumenta un grado de multiprogramacion, se pasa a estado EXIT
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
                // liberar_proceso_en_memoria(proceso_exit->pid); (el proceso no se carga a MEMORIA hasta que vaya a READY)
                log_cambio_estado_proceso(logger, proceso_exit->pid, "NEW", "EXIT");
                sem_post(&mutex_planificacion_pausada[3]);
                break;
            case READY:
            case EXEC:
            case BLOCKED:
                // ya desalojó correctamente desde donde correspondía (en caso de estado EXEC)
                op_code estado_actual = proceso_exit->estado;
                liberar_recursos(proceso_exit); // puede asignar procesos a READY (BLOCKED -> READY) => va antes de liberar el grado de multiprogramación: (BLOCKED -> READY) > (NEW -> READY)
                liberar_proceso_en_memoria(proceso_exit->pid);// libero memoria => se le manda ORDEN para que libere los MARCOS y la tabla de páginas!
                log_cambio_estado_proceso(logger, proceso_exit->pid, (char *)estado_proceso_strings[estado_actual], "EXIT");
                sem_post(&mutex_planificacion_pausada[3]);
                proceso_exit->estado = EXIT;
                sem_post(&contador_grado_multiprogramacion); 
                break;
            default:
                break;
        }
        // puede ser que el proceso se haya mandado a finalizar cuando estaba bloqueado esperando un interfaz, en esos casos se debe liberar memoria
        if(proceso_exit->instruccion_io != NULL){
            buffer_destroy(proceso_exit->instruccion_io->buffer); // libera buffer del paquete
            free(proceso_exit->instruccion_io); // libera paquete
            proceso_exit->instruccion_io = NULL;
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
                sem_wait(&mutex_planificacion_pausada[2]); // control planificacion para mediano plazo!
                if(!mqueue_is_empty(cola_recursos_bloqueados[i])){
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
                    sem_post(&mutex_planificacion_pausada[2]); // control planificacion para mediano plazo!
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
    // estas variables son independientes para cada hilo => para cada interfaz conectada!!!
	int cliente_recibido = *(int*) cliente;
    t_interfaz* interfaz;
	while(1){
		int cod_op = recibir_operacion(cliente_recibido); // bloqueante
		switch (cod_op){
		case CONEXION:
			// 1. KERNEL recibe una conexion con una interfaz I/O 
            t_sbuffer* buffer_conexion = cargar_buffer(cliente_recibido);
            uint32_t length_nombre, length_tipo_interfaz;
            char *nombre_interfaz = buffer_read_string(buffer_conexion, &length_nombre);
            nombre_interfaz[strcspn(nombre_interfaz, "\n")] = '\0';
            char* tipo_interfaz = buffer_read_string(buffer_conexion, &length_tipo_interfaz);
            tipo_interfaz[strcspn(tipo_interfaz, "\n")] = '\0';

            // 2. CARGAR NUEVA T_INTERFAZ
            t_interfaz* interfaz_conectada = malloc(sizeof(t_interfaz));
            interfaz_conectada->nombre_interfaz = malloc(strlen(nombre_interfaz) + 1); // espacio para el '/0'
            strcpy(interfaz_conectada->nombre_interfaz, nombre_interfaz);
            interfaz_conectada->tipo_interfaz = malloc(strlen(tipo_interfaz) + 1);
            strcpy(interfaz_conectada->tipo_interfaz, tipo_interfaz);
            interfaz_conectada->socket_interfaz = cliente_recibido;
            interfaz_conectada->cola_bloqueados = mqueue_create();
            interfaz_conectada->disponibilidad = 0; 
            interfaz_conectada->pid_ejecutando = 0; // por ponerle un valor, en realidad no importa esto en esta instancia y no modifica el resto

            free(nombre_interfaz);
            free(tipo_interfaz);

            // 2. Agregar interfaz a la lista
            sem_wait(&mutex_interfaces_conectadas);
            list_add(interfaces_conectadas, interfaz_conectada);
            sem_post(&mutex_interfaces_conectadas);
            interfaz = interfaz_conectada;

            log_info(logger, "Se conecta la interfaz %s al socket %d.", nombre_interfaz, cliente_recibido);

            buffer_destroy(buffer_conexion);
			break;
        // TODAS LAS IO, luego de ejecutar exitosamente la instruccion, deben devolver el siguiente mensaje a KERNEL
        case IO_LIBERAR:
            log_debug(logger, "se desaloja interfaz %s.", interfaz->nombre_interfaz);
            sem_wait(&mutex_planificacion_pausada[2]); // continúa sólo si la planificación no está pausada
            
            // 1. desbloquear proceso actual y mandarlo a READY (si es VRR manda a READY+)
            sem_wait(&(interfaz->cola_bloqueados->mutex)); // por si se está ingresando un proceso desde el manejo del desalojo en corto plazo
            // puede pasar que el proceso se haya mandado a finalizar mientras estaba en la interfaz!!
            if(!queue_is_empty(interfaz->cola_bloqueados->cola)){
                t_pcb* proceso_io_consulta_exit = queue_peek(interfaz->cola_bloqueados->cola); 
                if(proceso_io_consulta_exit->pid == interfaz->pid_ejecutando){
                    t_pcb* proceso_io = queue_pop(interfaz->cola_bloqueados->cola);
                    proceso_io->estado = READY;

                    if(corresponde_timer_vrr == 1 && proceso_io->quantum < config.quantum){
                        mqueue_push(monitor_READY_VRR, proceso_io);
                        log_debug(logger, "se agrega a READY+ proceso %u", proceso_io->pid);
                    } else {
                        mqueue_push(monitor_READY, proceso_io);
                        log_debug(logger, "se agrega a READY proceso %u", proceso_io->pid );
                    }

                    log_cambio_estado_proceso(logger, proceso_io->pid, "BLOCKED", "READY");
                    sem_post(&orden_planificacion_corto_plazo); // RECORDAR ESTO PARA AVISARLE AL PLANI DE CORTO PLAZO QUE ENTRÓ ALGO EN READY
                }
            }
            sem_post(&mutex_planificacion_pausada[2]); // SE LIBERA PLANIFICACIÓN BLOCKED -> READY

            // 2. si hay un proceso bloqueado en la cola de esta interfaz, lo envías a la interfaz con el paquete que tenía cargado el proceso
            if(!queue_is_empty(interfaz->cola_bloqueados->cola)){
                t_pcb* proceso_io_bloqueado = queue_peek(interfaz->cola_bloqueados->cola); // NO lo saca -> lo saca de la cola cuando desaloja la IO desde IO_LIBERAR
                interfaz->pid_ejecutando = proceso_io_bloqueado->pid;
                
                void* a_enviar = malloc((proceso_io_bloqueado->instruccion_io->buffer->size) + sizeof(int) + sizeof(uint32_t)); // toma el buffer ya cargado para el proceso bloqueado
                int offset = 0;

                memcpy(a_enviar + offset, &(proceso_io_bloqueado->instruccion_io->codigo_operacion), sizeof(int));
                offset += sizeof(int);
                memcpy(a_enviar + offset, &(proceso_io_bloqueado->instruccion_io->buffer->size), sizeof(uint32_t));
                offset += sizeof(uint32_t);
                memcpy(a_enviar + offset, proceso_io_bloqueado->instruccion_io->buffer->stream, proceso_io_bloqueado->instruccion_io->buffer->size);

                // --------- se envía el paquete 
                send(interfaz->socket_interfaz, a_enviar, proceso_io_bloqueado->instruccion_io->buffer->size + sizeof(int) + sizeof(uint32_t), 0);

                // --------- se libera memoria
                free(a_enviar);
                buffer_destroy(proceso_io_bloqueado->instruccion_io->buffer); // libera buffer del paquete
                free(proceso_io_bloqueado->instruccion_io); // libera paquete
                proceso_io_bloqueado->instruccion_io = NULL; // para que no quede apuntando a nada
                
            } else {
                interfaz->disponibilidad = 0; // se vuelve a liberar la interfaz
            }
            sem_post(&(interfaz->cola_bloqueados->mutex));            
        break;
		case -1:
			log_debug(logger, "Se desconectó la interfaz %s", interfaz->nombre_interfaz);
            // cuando se desconecta una interfaz habría que hacer algo así:
            // 1. TODO: si la interfaz tenía procesos tomados/bloqueados mandarlos a EXIT!!!!
            
            // 2. Se lo saca de la lista de interfaces conectadas
            bool buscar_por_nombre(void* data) {
                return strcmp(((t_interfaz*)data)->nombre_interfaz, interfaz->nombre_interfaz) == 0;
            };

            sem_wait(&mutex_interfaces_conectadas);
            t_interfaz* interfaz_desconectada = (t_interfaz*)list_remove_by_condition(interfaces_conectadas, buscar_por_nombre);
            sem_post(&mutex_interfaces_conectadas);

            // 3. liberar memoria
            if(interfaz_desconectada != NULL){
                free(interfaz_desconectada->nombre_interfaz);
                free(interfaz_desconectada->tipo_interfaz);
                mqueue_destroy(interfaz_desconectada->cola_bloqueados);
                free(interfaz_desconectada);
            }

			close(cliente_recibido); // cierro el socket accept del cliente
			free(cliente); // libero el malloc reservado para el cliente desde servidor escucha
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
    recibir_operacion(conexion_memoria);
    log_debug(logger, "envio proceso a liberar a memoria");
} //FUNCION liberar_memoria_proceso VA EN MEMORIA TRAS RECIBIR ESTE COD_OP
// ------------- FIN FUNCIONES PARA MEMORIA -------------