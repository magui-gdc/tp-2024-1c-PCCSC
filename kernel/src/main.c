#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

config_struct config;
t_list* pcb_list; // lista dinámica que contiene los PCB de los procesos creados
uint32_t pid; // PID: contador para determinar el PID de cada proceso creado

int main(int argc, char* argv[]) {

    int conexion_cpu_dispatch, conexion_memoria, conexion_cpu_interrupt;
    pthread_t thread_kernel_servidor, thread_kernel_consola, thread_planificador_corto_plazo, thread_planificador_largo_plazo;

    // ------------ ARCHIVOS CONFIGURACION + LOGGER ------------
    t_config* archivo_config = iniciar_config("kernel.config");
    cargar_config_struct_KERNEL(archivo_config);
    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);

    pid = 0;
    
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
    //log_info(logger, config.puerto_escucha);
    log_info(logger, "Server KERNEL iniciado");
    

    // ------------ HILOS ------------
    // hilo con MULTIPLEXACION a Interfaces I/O
    if(pthread_create(&thread_kernel_servidor, NULL, servidor_escucha, &socket_servidor) != 0) {
        log_error(logger, "No se ha podido crear el hilo para la conexion con interfaces I/O");
        exit(EXIT_FAILURE);
    } 
    // hilo para recibir mensajes por consola
    if(pthread_create(&thread_kernel_consola, NULL, consola_kernel, archivo_config) != 0){
        log_error(logger, "No se ha podido crear el hilo para la consola kernel");
        exit(EXIT_FAILURE);
    }
    // hilo para planificacion a corto plazo (READY A EXEC)
    pthread_create(&thread_planificador_corto_plazo, NULL, planificar_corto_plazo, NULL);
    // hilo para planificacion a largo plazo (NEW A READY)
    pthread_create(&thread_planificador_largo_plazo, NULL, planificar_largo_plazo, NULL);
   
    pthread_join(thread_kernel_servidor, NULL);
    pthread_join(thread_kernel_consola, NULL);
    pthread_join(thread_planificador_corto_plazo, NULL);
    pthread_join(thread_planificador_largo_plazo, NULL);
    
    log_destroy(logger);
	config_destroy(archivo_config);
    liberar_conexion(conexion_memoria);
	liberar_conexion(conexion_cpu_dispatch); 
    liberar_conexion(conexion_cpu_interrupt);
    
    return EXIT_SUCCESS;
}

// ------------ DEFINICION FUNCIONES KERNEL ------------

void cargar_config_struct_KERNEL(t_config* archivo_config){
    config.puerto_escucha = config_get_string_value(archivo_config, "PUERTO_ESCUCHA");
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
    config.ip_cpu = config_get_string_value(archivo_config, "IP_CPU");
    config.puerto_cpu_dispatch = config_get_string_value(archivo_config, "PUERTO_CPU_DISPATCH");
    config.puerto_cpu_interrupt = config_get_string_value(archivo_config, "PUERTO_CPU_INTERRUPT");
    config.algoritmo_planificacion = config_get_string_value(archivo_config, "ALGORITMO_PLANIFICACION");
    config.quantum = config_get_int_value(archivo_config, "QUANTUM");
    // TODO: a partir de aca, en realidad no conviene guardarlos, porque los valores de estas claves se pueden modificar desde distintos hilos!
    // por ahora lo comento para analizar después
    /*config.recursos = config_get_array_value(archivo_config, "RECURSOS" );
    config.instancias = config_get_array_value(archivo_config, "INSTANCIAS");
    config.grado_multiprogramacion = config_get_int_value(archivo_config, "GRADO_MULTIPROGRAMACION");*/
}

// definicion funcion hilo consola
void* consola_kernel(void*archivo_config){
    char* leido;
    while(1){
        leido = readline("> ");

        // Verificar si se ingresó algo
        if(strlen(leido) == 0){
            free(leido);
            break;
        }

        char** tokens = string_split(leido, " ");
        char* comando = tokens[0];
        if(comando != NULL){
            if(strcmp(comando, "EJECUTAR_SCRIPT") == 0 && string_array_size(tokens) >= 2){
                char* path = tokens[1];
                if(strlen(path) != 0 && path != NULL ){
                    // comprobar existencia de archivo en ruta relativa
                    // ejecutar_script(path);
                    printf("path ingresado (ejecutar_script): %s\n", path);
                }
            } else if(strcmp(comando, "INICIAR_PROCESO") == 0 && string_array_size(tokens) >= 2){
                char* path = tokens[1];
                 if(strlen(path) != 0 && path != NULL ){
                    // iniciar_proceso(path):
                        // comprueba existencia de archivo en ruta relativa en MEMORIA ¿acá o durante ejecución? => revisar consigna
                        // crea un t_pcb, 
                        // le carga en el pid el valor de pid++ (actualizando valor de la variable)
                        // le carga el quantum cargado en config.quantum
                        // le carga el pc en 0 (?)
                        // le carga el estado en NEW
                        // list_add(pcb_list, new_pcb);
                        // lo agrega en la cola NEW --> consultada desde planificador_largo_plazo, evaluar semáforo. 
                    printf("path ingresado (iniciar_proceso): %s\n", path);
                }
            } else if(strcmp(comando, "FINALIZAR_PROCESO") == 0 && string_array_size(tokens) >= 2){
                char* pid = tokens[1];
                if(strlen(pid) != 0 && pid != NULL && atoi(pid) > 0){
                    // finalizar_proceso(pid);
                    printf("pid ingresado (finalizar_proceso): %s\n", pid);
                }
            } else if(strcmp(comando, "MULTIPROGRAMACION") == 0 && string_array_size(tokens) >= 2){
                char* valor = tokens[1];
                if(strlen(valor) != 0 && valor != NULL && atoi(valor) > 0){
                    config_set_value((t_config*) archivo_config,"GRADO_MULTIPROGRAMACION", valor);
                    // WAIT MUTEX ACA!! => porque esta informacion es consultada desde PLANIFICACION_LARGO_PLAZO
                    config_save((t_config*) archivo_config);
                    // SIGNAL MUTEX ACA!! => porque esta informacion es consultada desde PLANIFICACION_LARGO_PLAZO
                    printf("grado multiprogramacion cambiado a %s\n", valor);
                }
            } else if(strcmp(comando, "DETENER_PLANIFICACION") == 0){
                // detener_planificacion()
                printf("detener planificacion\n");
            } else if(strcmp(comando, "INICIAR_PLANIFICACION") == 0){
                // iniciar_planificacion()
                printf("iniciar planificacion\n");
            } else if(strcmp(comando, "PROCESO_ESTADO") == 0){
                // estados_procesos()
                printf("estados de los procesos\n");
            }
        }
        string_array_destroy(tokens);
        free(leido);
    }

    return NULL;
}

void* planificar_corto_plazo(void* arg){
    while(1){
        // PLANIFICACION corto plazo
    }
}

void* planificar_largo_plazo(void* arg){
    while(1){
        // PLANIFICACION Largo plazo
    }
}
