#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <pthread.h>
#include "main.h"
#include "instrucciones.h"

config_struct config;
t_registros_cpu registros; 
int conexion_memoria; // SOCKET
int seguir_ejecucion = 1, proceso_instruccion_exit = 0; //FLAGS
uint32_t pid_proceso; // PID PROCESO EN EJECUCIÓN

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
    int cliente_kernel = esperar_cliente(socket_servidor_dispatch); 
    while(1){
        int cod_op = recibir_operacion(cliente_kernel);
        // DESDE ACA SE MANEJAN EJECUCIONES DE PROCESOS A DEMANDA DE KERNEL 
        switch (cod_op){
        case CONEXION:
            recibir_conexion(cliente_kernel);
            break;
        case EJECUTAR_PROCESO:
            seguir_ejecucion = 1, proceso_instruccion_exit = 0;
            log_info(logger, "RECIBISTE ALGO EN EJECUTAR_PROCESO");
            
            // guarda BUFFER del paquete enviado
            t_sbuffer *buffer_dispatch = cargar_buffer(cliente_kernel);
            
            // guarda datos del buffer (contexto de proceso)
            pid_proceso = buffer_read_uint32(buffer_dispatch); // guardo PID del proceso que se va a ejecutar
            buffer_read_registros(buffer_dispatch, &(registros)); // cargo contexto del proceso en los registros de la CPU

            // a modo de log: CAMBIAR DESPUÉS
            char* mensaje = (char*)malloc(128);
            sprintf(mensaje, "Recibi para ejecutar el proceso %u, junto a PC %u", pid_proceso, registros.PC);
            log_info(logger, "%s", mensaje);
            free(mensaje);

            // comienzo ciclo instrucciones
            while(seguir_ejecucion){
                ciclo_instruccion(cliente_kernel); // supongo que lo busca con PID en memoria (ver si hay que pasar la PATH en realidad, y si ese dato debería ir en el buffer y en el pcb de kernel)
            }
            buffer_destroy(buffer_dispatch);
            break;

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

void ciclo_instruccion(int conexion_kernel){
    // ---------------------- ETAPA FETCH ---------------------- //
    // 1. FETCH: búsqueda de la sgte instrucción en MEMORIA (por valor del Program Counter pasado por el socket)
    // provisoriamente, le pasamos PID y PC a memoria, con codigo de operación LEER_PROCESO
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
        uint32_t length;
        char* leido = buffer_read_string(buffer_de_instruccion, &length);

        // ---------------------- ETAPA DECODE + EXECUTE  ---------------------- //
        // 2. DECODE: interpretar qué instrucción es la que se va a ejecutar y si la misma requiere de una traducción de dirección lógica a dirección física.
        ejecutar_instruccion(leido, conexion_kernel); 
        
        free(leido);
        buffer_destroy(buffer_de_instruccion);
        // ---------------------- CHECK INTERRUPT  ---------------------- //
        // 4. CHECK INTERRUPT: chequea interrupciones en PIC y las maneja (cola de interrupciones y las atiende por prioridad/orden ANALIZAR) 
        // deberia preguntar si llego algo al socket de interrupt para el PID que se esta ejecutando y manejarla si ya el proceso no ejecutó EXIT ¿? , es decir sólo cuando !proceso_instruccion_exit
        // si por algun momento se va a la mrd (INT, EXIT O INST como WAIT (ver si deja de ejecutar)): seguir_ejecutando = 0;

        // IMPORTANTE: ante error o el manejo de alguna interrupción, también se podría cortar con la ejecución del proceso y devolverlo a kernel SIEMPRE A TRAVÉS DEL PUERTO DISPATCH
        // Si no hay necesidad de abandonar la ejecución del proceso, una vez actualizado el contexto, suma 1 al Program Counter 
        registros.PC++;
    } else {
        // TODO: PROBLEMAS
    }
}

void ejecutar_instruccion(char* leido, int conexion_kernel) {
    // 2. DECODE: interpretar qué instrucción es la que se va a ejecutar y si la misma requiere de una traducción de dirección lógica a dirección física.
    // TODO: MMU en caso de traducción dire. lógica a dire. física
    leido[strcspn(leido, "\n")] = '\0';
    char *mensaje = (char *)malloc(128);
    sprintf(mensaje, "CPU: LINEA DE INSTRUCCION %s", leido);
    log_info(logger, "%s", mensaje);
    free(mensaje);

    char **tokens = string_split(leido, " ");
    char *comando = tokens[0];
    // ------------------------------- ETAPA EXECUTE ---------------------------------- //
    // 3. EXECUTE: ejecutar la instrucción, actualizando su contexto.  Considerar que ciertas INSTRUCCIONES deben mandar por puerto DISPATCH a KERNEL ciertas solicitudes, como por ejemplo, WAIT-SIGNAL-ACCEDER A I/O-EXIT ..., y deberán esperar la respuesta de KERNEL para seguir avanzando
    if (comando != NULL){
        if (strcmp(comando, "SET") == 0){
            char *parametro1 = tokens[1]; 
            char *parametro2 = tokens[2]; 
            set(parametro1, parametro2);
        } else 
        if (strcmp(comando, "SUM") == 0){
            char *parametro1 = tokens[1]; 
            char *parametro2 = tokens[2]; 
            SUM(parametro1, parametro2, logger);
        } else if (strcmp(comando, "SUB") == 0){
            char *parametro1 = tokens[1]; 
            char *parametro2 = tokens[2]; 
            SUB(parametro1, parametro2);
        } else if (strcmp(comando, "EXIT") == 0){
            seguir_ejecucion = 0;
            proceso_instruccion_exit = 1;
            t_sbuffer *buffer_desalojo = NULL;
            desalojo_proceso(&buffer_desalojo, conexion_kernel, EXIT_PROCESO);
            // en este caso, lo desaloja y no tiene que esperar a que devuelve algo KERNEL
        } else if (strcmp(comando, "JNZ") == 0){
            char *registro = tokens[1]; 
            char *proxInstruccion = tokens[2]; 
            jnz(registro, proxInstruccion);
        }
        // TODO: SEGUIR
    }
    string_array_destroy(tokens);
}

// solo carga el contexto y retorna proceso (si tuvo que cargar otro valor antes suponemos que ya venía cargado en el buffer que se pasa por parámetro)
void desalojo_proceso(t_sbuffer **buffer_contexto_proceso, int conexion_kernel, op_code mensaje_desalojo){
    // O. Creo buffer si no vino cargado => si vino cargado, se supone que ya tiene el size que considera los registros que se cargan en esta función. Esto tiene lógica para cuando se necesite pasar otros valores en el buffer además del contexto de ejecución, como por ejemplo en la INST WAIT que se pasa el recurso que se quiere utilizar. 
    if(!*buffer_contexto_proceso){ // por defecto, si no vino nada cargado, siempre desalojo con contexto de ejecucion = registros
        *buffer_contexto_proceso = buffer_create(
            sizeof(uint32_t) * 8 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI
            + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
        );
    }
    // 1. Cargo buffer con contexto de ejecución del proceso en el momento del desalojo
    buffer_add_registros(*buffer_contexto_proceso, &(registros));
    // 2. Envió el contexto de ejecución del proceso y el motivo de desalojo (código de operación del paquete) a KERNEL
    cargar_paquete(conexion_kernel, mensaje_desalojo, *buffer_contexto_proceso); // esto ya desaloja el buffer!
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
