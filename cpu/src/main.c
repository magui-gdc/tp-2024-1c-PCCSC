#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include <pthread.h>
#include "main.h"
#include "instrucciones.h"

t_list* tlb_list;
int TAM_MEMORIA;
int TAM_PAGINA;

config_struct config;
t_registros_cpu registros; 
int conexion_memoria; // SOCKET
int seguir_ejecucion = 1, desalojo = 0; //FLAGS
uint32_t pid_proceso; // PID PROCESO EN EJECUCIÓN
t_list* list_interrupciones;
sem_t mutex_lista_interrupciones;

int main(int argc, char* argv[]) {
    

    // ------------ ARCHIVOS CONFIGURACION + LOGGER ------------
    pthread_t thread_interrupt; //, thread_dispatch;
    t_config* archivo_config = iniciar_config("cpu.config");    
    cargar_config_struct_CPU(archivo_config);
    logger = log_create("cpu.log", "CPU", 1, LOG_LEVEL_DEBUG);

    tlb_list = list_create();
    list_interrupciones = list_create();

    sem_init(&mutex_lista_interrupciones, 0, 1);

    decir_hola("CPU");

    // ------------ CONEXION CLIENTE - SERVIDORES ------------
    // conexion memoria
    conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);
    enviar_conexion("CPU", conexion_memoria);

    // le pido los valores de TAM_MEMORIA y TAM_PAGINA
    int operacion = DATOS_MEMORIA;
    send(conexion_memoria, &operacion, sizeof(operacion), 0);
    recibir_operacion(conexion_memoria); // espera a recibir los datos desde memoria
    t_sbuffer *buffer_memoria = cargar_buffer(conexion_memoria);
    TAM_MEMORIA = buffer_read_int(buffer_memoria);
    TAM_PAGINA  = buffer_read_int(buffer_memoria);
    buffer_destroy(buffer_memoria);
    log_debug(logger, "obtengo valores de tam_memoria %d y de tam_pagina %d", TAM_MEMORIA, TAM_PAGINA);

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
            seguir_ejecucion = 1;
            desalojo = 0;
            log_debug(logger, "RECIBISTE ALGO EN EJECUTAR_PROCESO");
            
            // guarda BUFFER del paquete enviado
            t_sbuffer *buffer_dispatch = cargar_buffer(cliente_kernel);
            
            // guarda datos del buffer (contexto de proceso)
            pid_proceso = buffer_read_uint32(buffer_dispatch); // guardo PID del proceso que se va a ejecutar
            buffer_read_registros(buffer_dispatch, &(registros)); // cargo contexto del proceso en los registros de la CPU

            // a modo de log: CAMBIAR DESPUÉS
            log_debug(logger, "Recibi para ejecutar el proceso %u, junto a PC %u", pid_proceso, registros.PC);

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
    list_destroy(tlb_list);
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
    log_debug(logger, "envio orden lectura a memoria");

    // TODO: CPU ESPERA POR DETERMINADO TIEMPO A MEMORIA, y sino sigue ¿?
    if(recibir_operacion(conexion_memoria) == INSTRUCCION){
        t_sbuffer *buffer_de_instruccion = cargar_buffer(conexion_memoria);
        uint32_t length;
        char* leido = buffer_read_string(buffer_de_instruccion, &length);
        leido[strcspn(leido, "\n")] = '\0'; // CORREGIR: DEBE SER UN PROBLEMA DESDE EL ENVÍO DEL BUFFER!

        // ---------------------- ETAPA DECODE + EXECUTE  ---------------------- //
        // 2. DECODE: interpretar qué instrucción es la que se va a ejecutar y si la misma requiere de una traducción de dirección lógica a dirección física.
        ejecutar_instruccion(leido, conexion_kernel); 
        
        // ---------------------- CHECK INTERRUPT  ---------------------- //
        // 4. CHECK INTERRUPT: chequea interrupciones en PIC y las maneja (cola de interrupciones y las atiende por prioridad/orden ANALIZAR) 
        check_interrupt(pid_proceso, conexion_kernel);
      
        // IMPORTANTE: si por algun motivo se va a la mrd (INT, EXIT O INST como WAIT (ver si deja de ejecutar)): seguir_ejecutando = 0 + desalojo = 1;

        // libero recursos
        free(leido);
        buffer_destroy(buffer_de_instruccion);
    } else {
        // TODO: PROBLEMAS
    }
}

bool coinciden_pid(void* element, uint32_t pid){
    return ((t_pic*)element)->pid == pid;
}

bool comparar_prioridad(void* a, void* b) {
    t_pic* pic_a = (t_pic*)a;
    t_pic* pic_b = (t_pic*)b;
    return pic_a->bit_prioridad < pic_b->bit_prioridad;
}

t_list* filtrar_y_remover_lista(t_list* lista_original, bool(*condicion)(void*, uint32_t), uint32_t pid){
    t_list* lista_filtrada = list_create();
    sem_wait(&mutex_lista_interrupciones);
    t_list_iterator* iterator = list_iterator_create(lista_original);

    while (list_iterator_has_next(iterator)) {
        // Obtener el siguiente elemento de la lista original
        void* element = list_iterator_next(iterator);

        // Verificar si el elemento cumple con la condición
        if (condicion(element, pid)) {
            // Agregar el elemento a la lista filtrada
            list_add(lista_filtrada, element);
            // Remover el elemento de la lista original sin destruirlo
            list_iterator_remove(iterator);
        }
    }
    sem_post(&mutex_lista_interrupciones);

    list_iterator_destroy(iterator);
    return lista_filtrada;
}

void check_interrupt(uint32_t proceso_pid, int conexion_kernel){
    log_debug(logger, "chequeando interrupciones");
    sem_wait(&mutex_lista_interrupciones);
    if(!list_is_empty(list_interrupciones)){
        sem_post(&mutex_lista_interrupciones);
        log_debug(logger, "hay interupciones");
        // remueve las interrupciones del proceso actual aun si el proceso ya fue desalojado => para que no se traten en la prox. ejecucion (si es que vuelve a ejecutar)
        t_list* interrupciones_proceso_actual = filtrar_y_remover_lista(list_interrupciones, coinciden_pid, proceso_pid);
        if(!list_is_empty(interrupciones_proceso_actual) && desalojo == 0){ // procesamos la interrupcion si todavia no se desalojo la ejecucion del proceso durante el proceso de EJECUCION
            log_debug(logger, "hay interrupciones para el pid seleccionado");
            list_sort(interrupciones_proceso_actual, comparar_prioridad);
            // solo proceso la interrupcion de mas prioridad, que sera la primera!
            seguir_ejecucion = 0;
            t_pic* primera_interrupcion = (t_pic*)list_get(interrupciones_proceso_actual, 0);
            t_sbuffer *buffer_desalojo_interrupt = NULL;
            desalojo_proceso(&buffer_desalojo_interrupt, conexion_kernel, primera_interrupcion->motivo_interrupcion);

            log_debug(logger, "Desaloje el proceso %u, por INT %d", primera_interrupcion->pid, primera_interrupcion->motivo_interrupcion);
        }
        list_clean_and_destroy_elements(interrupciones_proceso_actual, free); // Libera cada elemento en la lista filtrada
        list_destroy(interrupciones_proceso_actual);
    } else 
        sem_post(&mutex_lista_interrupciones);
}

void ejecutar_instruccion(char* leido, int conexion_kernel) {
    // 2. DECODE: interpretar qué instrucción es la que se va a ejecutar y si la misma requiere de una traducción de dirección lógica a dirección física.
    // TODO: MMU en caso de traducción dire. lógica a dire. física
    log_debug(logger, "CPU: LINEA DE INSTRUCCION %s", leido);

    char **tokens = string_split(leido, " ");
    char *comando = tokens[0];
    // ------------------------------- ETAPA EXECUTE ---------------------------------- //
    // 3. EXECUTE: ejecutar la instrucción, actualizando su contexto.  Considerar que ciertas INSTRUCCIONES deben mandar por puerto DISPATCH a KERNEL ciertas solicitudes, como por ejemplo, WAIT-SIGNAL-ACCEDER A I/O-EXIT ..., y deberán esperar la respuesta de KERNEL para seguir avanzando
    if (comando != NULL){
        registros.PC++; // Se suma 1 al Program Counter (por si desaloja para que desaloje con el PC actualizado!)

        if (strcmp(comando, "SET") == 0){
            char *parametro1 = tokens[1]; 
            char *parametro2 = tokens[2]; 
            set(parametro1, parametro2);
        } else 
        if (strcmp(comando, "SUM") == 0){
            char *parametro1 = tokens[1]; 
            char *parametro2 = tokens[2]; 
            SUM(parametro1, parametro2);
        } else if (strcmp(comando, "SUB") == 0){
            char *parametro1 = tokens[1]; 
            char *parametro2 = tokens[2]; 
            SUB(parametro1, parametro2);
        } else if (strcmp(comando, "JNZ") == 0){
            char *registro = tokens[1]; 
            char *proxInstruccion = tokens[2]; 
            jnz(registro, proxInstruccion);
        } else if (strcmp(comando, "EXIT") == 0){
            seguir_ejecucion = 0;
            desalojo = 1; // EN TODAS LAS INT donde se DESALOJA EL PROCESO cargar 1 en esta variable!!
            t_sbuffer *buffer_desalojo = NULL;
            desalojo_proceso(&buffer_desalojo, conexion_kernel, EXIT_PROCESO);
        } else if (strcmp(comando, "RESIZE") == 0){
            int tamanio_en_bytes = atoi(tokens[1]); 
            t_sbuffer* buffer_memoria_resize = buffer_create(
                sizeof(uint32_t) + // pid 
                sizeof(int) // resize
            );
            buffer_add_uint32(buffer_memoria_resize, pid_proceso);
            buffer_add_int(buffer_memoria_resize, tamanio_en_bytes);
            cargar_paquete(conexion_memoria, RESIZE, buffer_memoria_resize);
            int respuesta_resize_memoria = recibir_operacion(conexion_memoria);
            switch (respuesta_resize_memoria){
            case OUT_OF_MEMORY:
                seguir_ejecucion = 0;
                desalojo = 1;
                t_sbuffer *buffer_out_of_memory = NULL;
                desalojo_proceso(&buffer_out_of_memory, conexion_kernel, OUT_OF_MEMORY);
                break;
            default:
                break;
            }
        } else if (strcmp(comando, "MOV_OUT") == 0){
            char* registro_direccion = tokens[1];
            char* registro_dato = tokens[2]; // donde toma el dato para escribir
            uint32_t direccion_logica = obtener_valor_registro(registro_direccion);
            uint32_t bytes_a_escribir = (obtenerTipo(registro_dato) == _UINT8) ? 1 : 4; // 1 byte si es uint8 y 4 bytes si es uint32
            uint32_t valor_a_escribir = obtener_valor_registro(registro_dato);

            // mando a MMU para que calcule dir. física y me retorne un buffer ya cargado con estos valores!
            t_respuesta_mmu respuesta_mmu = mmu("ESCRIBIR", direccion_logica, bytes_a_escribir, valor_a_escribir);
            
            cargar_paquete(conexion_memoria, PETICION_ESCRITURA, respuesta_mmu.buffer_peticiones);
            int recibir_operacion_memoria = recibir_operacion(conexion_memoria); // espera respuesta de memoria
            /* analizar si le puede mandar algún error
            switch (recibir_operacion_memoria){
            case ERROR_MEMORIA: 
                seguir_ejecucion = 0;
                desalojo = 1;
                break;
            default:
                break;
            }
            */
        } else if (strcmp(comando, "MOV_IN") == 0){
            char* registro_dato = tokens[1]; // donde va a almacenar el valor que lee de memoria
            char* registro_direccion = tokens[2];
            uint32_t direccion_logica = obtener_valor_registro(registro_direccion);
            uint32_t bytes_a_leer = (obtenerTipo(registro_dato) == _UINT8) ? 1 : 4; // 1 byte si es uint8 y 4 bytes si es uint32

            // mando a MMU para que calcule dir. física y me retorne un buffer ya cargado con estos valores!
            t_respuesta_mmu respuesta_mmu = mmu("LEER", direccion_logica, bytes_a_leer, 0);
            
            cargar_paquete(conexion_memoria, PETICION_LECTURA, respuesta_mmu.buffer_peticiones);
            int recibir_operacion_memoria = recibir_operacion(conexion_memoria); // espera respuesta de memoria
            switch (recibir_operacion_memoria){
                /* analizar si le puede mandar algún error
                    case ERROR_MEMORIA: 
                        seguir_ejecucion = 0;
                        desalojo = 1;
                    break;
                */
            case CONTINUAR: 
                // TODO: acá le va a responder con un buffer cargado con el valor leído de memoria, y la idea es que se lo guarde en el registro de la instruccion 
                // por cada peticion de lectura se devuelve un contenido del buffer, la cantidad de peticiones la obtenemos a partir del dato que devolvió el mmu
                int cantidad_peticiones = respuesta_mmu.cantidad_peticiones;
                // con esto haríamos un for y leeríamos uno a uno los datos (ya que quizás vienen divididos) para escribirlo en el registro
                break;
            }
        } else if (strcmp(comando, "WAIT") == 0 || strcmp(comando, "SIGNAL") == 0){
            char *recurso = tokens[1];
            op_code instruccion_recurso = (strcmp(comando, "WAIT") == 0) ? WAIT_RECURSO : SIGNAL_RECURSO;
            t_sbuffer *buffer_desalojo_wait = buffer_create(
                (uint32_t)strlen(recurso) + sizeof(uint32_t) + //la longitud del string
                sizeof(uint32_t) * 8 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI
                + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
            );
            buffer_add_string(buffer_desalojo_wait, (uint32_t)strlen(recurso), recurso);

            log_debug(logger, "Recurso pedido a kernel %s", recurso);

            desalojo_proceso(&buffer_desalojo_wait, conexion_kernel, instruccion_recurso); // agrega ctx en el buffer y envía paquete a kernel

            int respuesta = recibir_operacion(conexion_kernel); // BLOQUEANTE: espera respuesta de kernel
            switch (respuesta){
            case DESALOJAR:
                seguir_ejecucion = 0;
                desalojo = 1; // EN TODAS LAS INT donde se DESALOJA EL PROCESO cargar 1 en esta variable!!
                log_debug(logger, "desalojar proceso");
                break;
            default:
                // en caso de que la respuesta sea CONTINUAR, se continúa ejecutando normalmente
                break;
            }
        }
        ///////////////////////////// INSTRUCCIONES DE IO ///////////////////////////// 
        else if (strcmp(comando, "IO_GEN_SLEEP") == 0){
            char* nombre_interfaz = tokens[1];
            uint32_t tiempo_sleep = (uint32_t)atoi(tokens[2]);
            //io_gen_sleep(nombre_interfaz, tiempo_sleep);
            t_sbuffer *buffer_interfaz_gen_sleep = buffer_create(
                (uint32_t)strlen(nombre_interfaz) + sizeof(uint32_t) +
                sizeof(uint32_t) +
                sizeof(uint32_t) * 8 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI
                + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
            );

            buffer_add_string(buffer_interfaz_gen_sleep, (uint32_t)strlen(nombre_interfaz), nombre_interfaz);
            buffer_add_uint32(buffer_interfaz_gen_sleep, tiempo_sleep);
            desalojo_proceso(&buffer_interfaz_gen_sleep, conexion_kernel, IO_GEN_SLEEP); // agrega ctx en el buffer y envía paquete a kernel

            // EN INST. IO no hace falta esperar respuesta de KERNEL ya que siempre se bloquea o se manda a exit en caso de error!
            seguir_ejecucion = 0;
            desalojo = 1; 
            
        } else if (strcmp(comando, "IO_STDIN_READ") == 0){
            char* nombre_interfaz = tokens[1];
            uint32_t reg_d = obtener_valor_registro(tokens[2]);
            uint32_t reg_t = obtener_valor_registro(tokens[3]);
            //io_stdin_read(nombre_interfaz, registro_direccion, registro_tamaño);
            t_sbuffer *buffer_interfaz_stdin_read = buffer_create(
                (uint32_t)strlen(nombre_interfaz) + sizeof(uint32_t) +
                sizeof(uint32_t) * 2 // REGISTROS PARA IO
                + sizeof(uint32_t) * 8 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI
                + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
            );

            buffer_add_string(buffer_interfaz_stdin_read, (uint32_t)strlen(nombre_interfaz), nombre_interfaz);
            buffer_add_uint32(buffer_interfaz_stdin_read, reg_d);
            buffer_add_uint32(buffer_interfaz_stdin_read, reg_t);
            desalojo_proceso(&buffer_interfaz_stdin_read, conexion_kernel, IO_STDIN_READ); // agrega ctx en el buffer y envía paquete a kernel

            // EN INST. IO no hace falta esperar respuesta de KERNEL ya que siempre se bloquea o se manda a exit en caso de error!
            seguir_ejecucion = 0;
            desalojo = 1; 
            
        } else if (strcmp(comando, "IO_STDOUT_WRITE") == 0){
            char* nombre_interfaz = tokens[1];
            uint32_t reg_d = obtener_valor_registro(tokens[2]);
            uint32_t reg_t = obtener_valor_registro(tokens[3]);
            //io_stdout_write(nombre_interfaz, registro_direccion, registro_tamaño);
            t_sbuffer *buffer_interfaz_stdout_write = buffer_create(
                (uint32_t)strlen(nombre_interfaz) + sizeof(uint32_t) +
                sizeof(uint32_t) * 2 // REGISTROS PARA IO
                + sizeof(uint32_t) * 8 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI
                + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
            );

            buffer_add_string(buffer_interfaz_stdout_write, (uint32_t)strlen(nombre_interfaz), nombre_interfaz);
            buffer_add_uint32(buffer_interfaz_stdout_write, reg_d);
            buffer_add_uint32(buffer_interfaz_stdout_write, reg_t);
            desalojo_proceso(&buffer_interfaz_stdout_write, conexion_kernel, IO_STDOUT_WRITE); // agrega ctx en el buffer y envía paquete a kernel

            // EN INST. IO no hace falta esperar respuesta de KERNEL ya que siempre se bloquea o se manda a exit en caso de error!
            seguir_ejecucion = 0;
            desalojo = 1; 

        } else if (strcmp(comando, "IO_FS_CREATE") == 0){
            char* nombre_interfaz = tokens[1];
            char* nombre_file = tokens[2];
            //io_fs_create(nombre_interfaz, nombre_archivo);
            t_sbuffer *buffer_interfaz_fs_create = buffer_create(
                (uint32_t)strlen(nombre_interfaz) + sizeof(uint32_t)
                + (uint32_t)strlen(nombre_file) + sizeof(uint32_t)  // NOMBRE DE ARCHIVO
                + sizeof(uint32_t) * 8 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI
                + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
            );

            buffer_add_string(buffer_interfaz_fs_create, (uint32_t)strlen(nombre_interfaz), nombre_interfaz);
            buffer_add_string(buffer_interfaz_fs_create, (uint32_t)strlen(nombre_file), nombre_file);
            desalojo_proceso(&buffer_interfaz_fs_create, conexion_kernel, IO_FS_CREATE); // agrega ctx en el buffer y envía paquete a kernel

            // EN INST. IO no hace falta esperar respuesta de KERNEL ya que siempre se bloquea o se manda a exit en caso de error!
            seguir_ejecucion = 0;
            desalojo = 1; 

        } else if (strcmp(comando, "IO_FS_DELETE") == 0){
            char* nombre_interfaz = tokens[1];
            char* nombre_file = tokens[2];
            //io_fs_delete(nombre_interfaz, nombre_archivo);
            t_sbuffer *buffer_interfaz_fs_delete = buffer_create(
                (uint32_t)strlen(nombre_interfaz) + sizeof(uint32_t)
                + (uint32_t)strlen(nombre_file) + sizeof(uint32_t)  // NOMBRE DE ARCHIVO
                + sizeof(uint32_t) * 8 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI
                + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
            );

            buffer_add_string(buffer_interfaz_fs_delete, (uint32_t)strlen(nombre_interfaz), nombre_interfaz);
            buffer_add_string(buffer_interfaz_fs_delete, (uint32_t)strlen(nombre_file), nombre_file);
            desalojo_proceso(&buffer_interfaz_fs_delete, conexion_kernel, IO_FS_DELETE); // agrega ctx en el buffer y envía paquete a kernel

            // EN INST. IO no hace falta esperar respuesta de KERNEL ya que siempre se bloquea o se manda a exit en caso de error!
            seguir_ejecucion = 0;
            desalojo = 1; 

        } else if (strcmp(comando, "IO_FS_TRUNCATE") == 0){
            char* nombre_interfaz = tokens[1];
            char* nombre_file = tokens[2];
            uint32_t reg_t = obtener_valor_registro(tokens[3]);
            //io_fs_truncate(nombre_interfaz, nombre_archivo, registro tamaño);
            t_sbuffer *buffer_interfaz_fs_truncate = buffer_create(
                (uint32_t)strlen(nombre_interfaz) + sizeof(uint32_t)
                + (uint32_t)strlen(nombre_file) + sizeof(uint32_t)  // NOMBRE DE ARCHIVO
                + sizeof(uint32_t) // REGISTRO PARA IO
                + sizeof(uint32_t) * 8 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI
                + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
            );

            buffer_add_string(buffer_interfaz_fs_truncate, (uint32_t)strlen(nombre_interfaz), nombre_interfaz);
            buffer_add_string(buffer_interfaz_fs_truncate, (uint32_t)strlen(nombre_file), nombre_file);
            buffer_add_uint32(buffer_interfaz_fs_truncate, reg_t);
            desalojo_proceso(&buffer_interfaz_fs_truncate, conexion_kernel, IO_FS_TRUNCATE); // agrega ctx en el buffer y envía paquete a kernel

            // EN INST. IO no hace falta esperar respuesta de KERNEL ya que siempre se bloquea o se manda a exit en caso de error!
            seguir_ejecucion = 0;
            desalojo = 1; 
            
        } else if (strcmp(comando, "IO_FS_WRITE") == 0){
            char* nombre_interfaz = tokens[1];
            char* nombre_file = tokens[2];
            uint32_t reg_d = obtener_valor_registro(tokens[3]);
            uint32_t reg_t = obtener_valor_registro(tokens[4]);
            uint32_t reg_p = obtener_valor_registro(tokens[5]); // no estoy seguro que sea un int esto, porque es un registro "puntero archivo"
            //io_fs_write(nombre_interfaz, nombre_archivo, registro_direccion, registro_tamaño, registro_puntero_archivo);
            t_sbuffer *buffer_interfaz_fs_write = buffer_create(
                (uint32_t)strlen(nombre_interfaz) + sizeof(uint32_t)
                + (uint32_t)strlen(nombre_file) + sizeof(uint32_t)  // NOMBRE DE ARCHIVO
                + sizeof(uint32_t) * 3 // REGISTRO PARA IO
                + sizeof(uint32_t) * 8 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI
                + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
            );

            buffer_add_string(buffer_interfaz_fs_write, (uint32_t)strlen(nombre_interfaz), nombre_interfaz);
            buffer_add_string(buffer_interfaz_fs_write, (uint32_t)strlen(nombre_file), nombre_file);
            buffer_add_uint32(buffer_interfaz_fs_write, reg_d);
            buffer_add_uint32(buffer_interfaz_fs_write, reg_t);
            buffer_add_uint32(buffer_interfaz_fs_write, reg_p);
            desalojo_proceso(&buffer_interfaz_fs_write, conexion_kernel, IO_FS_WRITE); // agrega ctx en el buffer y envía paquete a kernel

            // EN INST. IO no hace falta esperar respuesta de KERNEL ya que siempre se bloquea o se manda a exit en caso de error!
            seguir_ejecucion = 0;
            desalojo = 1; 
            
        } else if (strcmp(comando, "IO_FS_READ") == 0){
            char* nombre_interfaz = tokens[1];
            char* nombre_file = tokens[2];
            uint32_t reg_d = obtener_valor_registro(tokens[3]);
            uint32_t reg_t = obtener_valor_registro(tokens[4]);
            uint32_t reg_p = obtener_valor_registro(tokens[5]); // no estoy seguro que sea un int esto, porque es un registro "puntero archivo"
            //io_fs_read(nombre_interfaz, nombre_archivo, registro_direccion, registro_tamaño, registro_puntero_archivo);
            t_sbuffer *buffer_interfaz_fs_read = buffer_create(
                (uint32_t)strlen(nombre_interfaz) + sizeof(uint32_t)
                + (uint32_t)strlen(nombre_file) + sizeof(uint32_t)  // NOMBRE DE ARCHIVO
                + sizeof(uint32_t) * 3 // REGISTRO PARA IO
                + sizeof(uint32_t) * 8 // REGISTROS: PC, EAX, EBX, ECX, EDX, SI, DI
                + sizeof(uint8_t) * 4 // REGISTROS: AX, BX, CX, DX
            );

            buffer_add_string(buffer_interfaz_fs_read, (uint32_t)strlen(nombre_interfaz), nombre_interfaz);
            buffer_add_string(buffer_interfaz_fs_read, (uint32_t)strlen(nombre_file), nombre_file);
            buffer_add_uint32(buffer_interfaz_fs_read, reg_d);
            buffer_add_uint32(buffer_interfaz_fs_read, reg_t);
            buffer_add_uint32(buffer_interfaz_fs_read, reg_p);
            desalojo_proceso(&buffer_interfaz_fs_read, conexion_kernel, IO_FS_READ); // agrega ctx en el buffer y envía paquete a kernel

            // EN INST. IO no hace falta esperar respuesta de KERNEL ya que siempre se bloquea o se manda a exit en caso de error!
            seguir_ejecucion = 0;
            desalojo = 1; 
            
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
    interrupcion_kernel = esperar_cliente(servidor_interrupt);
    while(1){
        int cod_op = recibir_operacion(interrupcion_kernel);
        switch (cod_op){
        case CONEXION:
            recibir_conexion(interrupcion_kernel);
            break;
        case INTERRUPCION:
            
            log_debug(logger, "RECIBISTE ALGO EN INTERRUPCION");
            t_sbuffer *buffer_interrupt = cargar_buffer(interrupcion_kernel);
            
            // guarda datos del buffer (contexto de proceso)
            t_pic *interrupcion_recibida = malloc(sizeof(t_pic));
            if(interrupcion_recibida != NULL){
                // guardo PID del proceso que se va a ejecutar
                interrupcion_recibida->pid = buffer_read_uint32(buffer_interrupt);
                interrupcion_recibida->motivo_interrupcion = buffer_read_int(buffer_interrupt);
                interrupcion_recibida->bit_prioridad = buffer_read_uint8(buffer_interrupt);
                
                // ANTES DE AGREGAR LA INTERRUPCION A LA LISTA DE INTERRUPCIONES, DEBE VERIFICAR QUE EL PROCESO NO SE HAYA DESALOJADO PREVIAMENTE
                sem_wait(&mutex_lista_interrupciones);
                list_add(list_interrupciones, interrupcion_recibida);
                sem_post(&mutex_lista_interrupciones);
                log_debug(logger, "Recibi una interrupcion para el proceso %u, por %d", interrupcion_recibida->pid, interrupcion_recibida->motivo_interrupcion);
            }
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

t_respuesta_mmu mmu(const char* operacion, uint32_t direccion_logica, uint32_t bytes_peticion, uint32_t dato_escribir) {
    int nro_pagina = (int)floor(direccion_logica/TAM_PAGINA);
    int desplazamiento = direccion_logica - (nro_pagina * TAM_PAGINA);
    
    int cantidad_peticiones_memoria = (int)ceil((desplazamiento + bytes_peticion)/TAM_PAGINA);  // necesitas leer/escribir X paginas
    t_sbuffer* buffer_direcciones_fisicas = buffer_create(
        sizeof(int) + // cantidad de peticiones
        sizeof(uint32_t) * cantidad_peticiones_memoria + // direccion/es fisica/s = nro_marco * tam_pagina + desplazamiento 
        sizeof(uint32_t) * cantidad_peticiones_memoria // bytes a leer/escribir por peticion
    );

    if(strcmp(operacion, "ESCRIBIR") == 0) {
    /* // TODO: este dato debería ser un VOID* ¿? ANALIZAR (dejo comentado por el sizeof)
        uint32_t new_size = buffer_direcciones_fisicas->size + sizeof(uint32_t) * cantidad_peticiones_memoria; // dato a escribir por peticion
        buffer_direcciones_fisicas->stream = realloc(buffer_direcciones_fisicas->stream, new_size);
        buffer_direcciones_fisicas->size = new_size;
    */
    }
    buffer_add_int(buffer_direcciones_fisicas, cantidad_peticiones_memoria);
    t_tlb* entrada_tlb;

    if(cantidad_peticiones_memoria == 1){
        entrada_tlb = buscar_marco_tlb(pid_proceso, nro_pagina);
        if(!entrada_tlb){
            // TODO: enviar petición a memoria para acceder a tabla de páginas pasando número de página y pid (espera respuesta informando marco)
            // TODO: agregar entrada a TLB según algoritmo de TLB
            // cargar en entrada_tlb la nueva entrada registrada para dicho proceso y página
        } 
        uint32_t direccion_fisica = entrada_tlb->marco * TAM_PAGINA + desplazamiento;
        buffer_add_uint32(buffer_direcciones_fisicas, direccion_fisica);
        buffer_add_uint32(buffer_direcciones_fisicas, bytes_peticion);
        // TODO: if(strcmp(operacion, "ESCRIBIR") == 0) buffer_add_uint32(buffer_direcciones_fisicas, dato_escribir); // TODO: analizar si está bien cómo se está pasando el tipo de dato para recibirlo correctamente del otro lado (esto porque en ese parámetro se pude haber tomado un valor tanto desde registro UINT8 como UINT32): creo que se debería pasar como un void* ver ISSUE https://github.com/sisoputnfrba/foro/issues/3781
    } else {
        uint32_t bytes_pendientes = bytes_peticion;
        uint32_t bytes_a_enviar_por_peticion;
        for (int i = 0; i < cantidad_peticiones_memoria; i++){
           if(i == 0) // en esta primera vuelta consumo el espacio restante a partir del offset de la primera página
                bytes_a_enviar_por_peticion = TAM_PAGINA - desplazamiento; // los bytes que entran en la primera página a partir del offset dado
           else // en las demás vueltas, consumo toda la página o lo que quede de los bytes pendientes ya que el offset es 0
                if(i < cantidad_peticiones_memoria)
                    bytes_a_enviar_por_peticion = TAM_PAGINA;
                else 
                    bytes_a_enviar_por_peticion = bytes_pendientes;
            

            if(strcmp(operacion, "ESCRIBIR") == 0) {
                void* datos_a_enviar;
                // TODO: acá debería tomar del contenido de datos_escribir los primeros N (=bytes_a_enviar_por_peticion) bytes y dejar el resto almacenado para la próxima peticion de escritura
                // todo se va a cargar en datos_a_enviar
            }

            entrada_tlb = buscar_marco_tlb(pid_proceso, nro_pagina);
            if(!entrada_tlb){
                // TODO: enviar petición a memoria para acceder a tabla de páginas pasando número de página y pid (espera respuesta informando marco)
                // TODO: agregar entrada a TLB según algoritmo de TLB
                // cargar en entrada_tlb la nueva entrada registrada para dicho proceso y página
            } 
            uint32_t direccion_fisica = entrada_tlb->marco * TAM_PAGINA + desplazamiento;
            buffer_add_uint32(buffer_direcciones_fisicas, direccion_fisica);
            buffer_add_uint32(buffer_direcciones_fisicas, bytes_a_enviar_por_peticion);
            // TODO -> ESCRIBIR => cómo se envía datos_a_enviar según lo cargado en los pasos anteriores   

            bytes_pendientes-= bytes_a_enviar_por_peticion;
            nro_pagina++; // próxima petición a la página siguiente
            desplazamiento = 0; // las páginas nuevas siempre parten de su offset 0
        }
    }
    t_respuesta_mmu respuesta;
    respuesta.cantidad_peticiones = cantidad_peticiones_memoria;
    respuesta.buffer_peticiones = buffer_direcciones_fisicas;
    return respuesta;
}

t_tlb* buscar_marco_tlb(uint32_t proceso, uint32_t nro_pagina){
    bool comparar_pid_pagina(void *elemento){
        t_tlb *entrada_tlb = (t_tlb *)elemento;
        return (entrada_tlb->pid == proceso && entrada_tlb->pagina == nro_pagina);
    }
    t_tlb* encontrado = (t_tlb*)list_find(tlb_list, comparar_pid_pagina);
    return encontrado; 
}

// la dejo vacía y declarada porque, a pesar de que no la requeramos acá, como importamos el utilsServer.c necesita de una defición de esta función, hay que ver qué conviene más adelante
void* atender_cliente(void* cliente){
    return NULL;
}
