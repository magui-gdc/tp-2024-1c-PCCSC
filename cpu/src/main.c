#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <pthread.h>
#include "main.h"
#include "instrucciones.h"

config_struct config;
t_registros_cpu registros; 
int conexion_memoria;
int seguir_ejecucion = 1;

int main(int argc, char* argv[]) {
    

    // ------------ ARCHIVOS CONFIGURACION + LOGGER ------------
    pthread_t thread_interrupt; //, thread_dispatch;
    t_config* archivo_config = iniciar_config("cpu.config");    
    cargar_config_struct_CPU(archivo_config);
    logger = log_create("cpu.log", "CPU", 1, LOG_LEVEL_DEBUG);

    decir_hola("CPU");

    // ------------ CONEXION CLIENTE - SERVIDORES ------------
    // conexion memoria
    conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);
    enviar_conexion("CPU", conexion_memoria);

    // ------------ CONEXION SERVIDOR - CLIENTES ------------
    // conexion dispatch
    int socket_servidor_dispatch = iniciar_servidor(config.puerto_escucha_dispatch);
    inicializar_registros();
    log_info(logger, config.puerto_escucha_dispatch);
    log_info(logger, "Server CPU DISPATCH");


    // conexion interrupt
    int socket_servidor_interrupt = iniciar_servidor(config.puerto_escucha_interrupt);
    log_info(logger, config.puerto_escucha_interrupt);
    log_info(logger, "Server CPU INTERRUPT"); 

    // hilo que recibe las interrupciones y las guarda en una 'lista' de interrupciones (PIC: Programmable Integrated Circuited) 
                                                // -> (ANALIZAR BAJO QUÉ CRITERIO: ¿se podría agregar ordenado por PID y BIT DE PRIORIDAD?)
    pthread_create(&thread_interrupt, NULL, recibir_interrupcion, &socket_servidor_interrupt); 
    pthread_detach(thread_interrupt);

    // NO hace falta un hilo para DISPATCH porque sólo KERNEL manda solicitudes a CPU, de forma SECUENCIAL (GRADO MULTIPROCESAMIENTO = 1)
    //pthread_create(&thread_dispatch, NULL, recibir_IO, &socket_servidor_dispatch); 
    //pthread_join(thread_dispatch, NULL);
    int cliente_kernel = esperar_cliente(socket_servidor_dispatch); 
    while(1){
        int cod_op = recibir_operacion(cliente_kernel);
        // DESDE ACA SE MANEJAN EJECUCIONES DE PROCESOS A DEMANDA DE KERNEL 
        // (ciclo de instrucciones: en el último paso se consulta por la cola de las interrupciones para tratarlas en caso de que se lo requiera)
        switch (cod_op)
        {
        case CONEXION:
            recibir_conexion(cliente_kernel);
            break;
        case EJECUTAR_PROCESO:
            seguir_ejecucion = 1;
            log_info(logger, "RECIBISTE ALGO EN EJECUTAR_PROCESO");
            
            // guarda BUFFER del paquete enviado
            t_sbuffer *buffer_dispatch = cargar_buffer(cliente_kernel);
            
            // guarda datos del buffer (contexto de proceso)
            uint32_t pid_proceso = buffer_read_uint32(buffer_dispatch); // guardo PID del proceso que se va a ejecutar
            buffer_read_registros(buffer_dispatch, &(registros)); // cargo contexto del proceso en los registros de la CPU

            // a modo de log: CAMBIAR DESPUÉS
            char* mensaje = (char*)malloc(128);
            sprintf(mensaje, "Recibi para ejecutar el proceso %u, junto a PC %u", pid_proceso, registros.PC);
            log_info(logger, "%s", mensaje);
            free(mensaje);

            // comienzo ciclo instrucciones
            while(seguir_ejecucion){
            ciclo_instruccion(pid_proceso); // supongo que lo busca con PID en memoria (ver si hay que pasar la PATH en realidad, y si esedato debería ir en el buffer y en el pcb de kernel)
            }
            buffer_destroy(buffer_dispatch);
            break;


            // función que ejecuta el proceso por ciclos de instrucción: necesidad de algún BUCLE que controle los cuatro pasos
            // hasta finalización, interrupción, bloqueo o error durante la ejecución del proceso
            /*
                1. FETCH: búsqueda de la sgte instrucción en MEMORIA (por valor del Program Counter pasado por el socket)
                2. DECODE: interpretar qué instrucción es la que se va a ejecutar y si la misma requiere de una traducción de dirección lógica a dirección física.
                3. EXECUTE: ejecutar la instrucción (se podría hacer un ENUM de todas las instrucciones posibles y por cada ENUM una función que determine qué acción 
                            se realiza y cómo se actualiza el contexto del PRC pasado por socket. Considerar que ciertas INSTRUCCIONES deben mandar por puerto DISPATCH 
                            a KERNEL ciertas solicitudes, como por ejemplo, WAIT-SIGNAL-ACCEDER A I/O-EXIT ..., y deberán esperar la respuesta de KERNEL para seguir avanzando
                4. CHECK INTERRUPT: chequea interrupciones en PIC y las maneja (cola de interrupciones y las atiende por prioridad/orden ANALIZAR)
                Si no hay necesidad de abandonar la ejecución del proceso, una vez actualizado el contexto, suma 1 al Program Counter 

                IMPORTANTE: ante error o el manejo de alguna interrupción, también se podría cortar con la ejecución del proceso y devolverlo a kernel SIEMPRE A TRAVÉS DEL PUERTO DISPATCH
            */

        case -1:
            log_error(logger, "cliente desconectado");
            break;
        default:
            log_warning(logger, "Operacion desconocida.");
            break;
        }
    }

    //Limpieza
    log_destroy(logger);
	config_destroy(archivo_config);
    liberar_conexion(conexion_memoria);

    return 0;
}

void cargar_config_struct_CPU(t_config* archivo_config){
    config.ip_memoria = config_get_string_value(archivo_config, "IP_MEMORIA");
    config.puerto_memoria = config_get_string_value(archivo_config, "PUERTO_MEMORIA");
    config.puerto_escucha_dispatch = config_get_string_value(archivo_config, "PUERTO_ESCUCHA_DISPATCH");
    config.puerto_escucha_interrupt = config_get_string_value(archivo_config, "PUERTO_ESCUCHA_INTERRUPT");
    config.cantidad_entradas_tlb = config_get_string_value(archivo_config, "CANTIDAD_ENTRADAS_TLB");
    config.algoritmo_tlb = config_get_string_value(archivo_config, "ALGORITMO_TLB");
}

void inicializar_registros(){
    registros.PC = 0;
    registros.AX = 0;
    registros.BX = 0;
    registros.CX = 0;
    registros.DX = 0;
    registros.EAX = 0;
    registros.EBX = 0;
    registros.ECX = 0;
    registros.EDX = 0;
    registros.SI = 0;
    registros.DI = 0;
}

void ciclo_instruccion(uint32_t pid_proceso){
    // ---------------------- ETAPA FETCH ---------------------- //
    // provisoriamente, le paso PID y PC a memoria, con codigo de operación ESCRIBIR_PROCESO
    t_sbuffer *buffer = buffer_create(
        sizeof(uint32_t) * 2 // PID + PC
    );

    buffer_add_uint32(buffer,pid_proceso);
    buffer_add_uint32(buffer, registros.PC);
    
    cargar_paquete(conexion_memoria, LEER_PROCESO, buffer); 
    log_info(logger, "envio orden lectura a memoria");
    // TODO: CPU ESPERA POR DETERMINADO TIEMPO A MEMORIA, y sino sigue ¿?
    if(recibir_operacion(conexion_memoria) == INSTRUCCION){
        t_sbuffer *buffer_de_instruccion = cargar_buffer(conexion_memoria);
        // ---------------------- ETAPA DECODE ---------------------- //
        decode_instruccion(buffer_de_instruccion, logger); // ---------------------- EXECUTE ------------------- //
        // chequear INTERRUPCCION: deberia preguntar si llego algo al socket de interrupt para el PID que se esta ejecutando
        // si por algun momento se va a la mrd (INT, EXIT O INST como WAIT (ver si deja de ejecutar)): seguir_ejecutando = 0;
        registros.PC++;
    } else {
        // TODO: PROBLEMAS
    }
}



void* recibir_interrupcion(void* conexion){
    int interrupcion_kernel, servidor_interrupt = *(int*) conexion;
    while((interrupcion_kernel = esperar_cliente(servidor_interrupt)) != -1){
        int cod_op = recibir_operacion(interrupcion_kernel);
        switch (cod_op)
        {
        case CONEXION:
            recibir_conexion(interrupcion_kernel);
            break;
        case INTERRUPCION:
            // acá recibe la interrupción y la agrega en la estructura PIC (evaluar si es una lista con struct donde se guarden valores como el PID del proceso, la acción a tomar, y si se debe agregar ordenado según algún bit de prioridad)
            break;
        case -1:
            log_error(logger, "cliente desconectado");
            break;
        default:
            log_warning(logger, "Operacion desconocida.");
            break;
        }
    }
    return NULL;
}

// la dejo vacía y declarada porque, a pesar de que no la requeramos acá, como importamos el utilsServer.c necesita de una defición de esta función, hay que ver qué conviene más adelante
void* atender_cliente(void* cliente){
    return NULL;
}
