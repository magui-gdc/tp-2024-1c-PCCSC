#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>
#include "main.h"

config_struct config;
pthread_t thread_memoria;

int main(int argc, char *argv[])
{

    t_config *archivo_config = iniciar_config("memoria.config");
    cargar_config_struct_MEMORIA(archivo_config);
    logger = log_create("memoria.log", "Servidor Memoria", 1, LOG_LEVEL_DEBUG);

    decir_hola("Memoria");

    // iniciar servidor memoria

    int socket_servidor = iniciar_servidor(config.puerto_escucha);
    log_info(logger, config.puerto_escucha);
    log_info(logger, "Server MEMORIA iniciado");

    if (pthread_create(&thread_memoria, NULL, servidor_escucha, &socket_servidor) != 0)
    {
        log_error(logger, "No se ha podido crear el hilo SERVIDOR de MEMORIA");
        exit(EXIT_FAILURE);
    }

    pthread_join(thread_memoria, NULL);

    log_destroy(logger);
    config_destroy(archivo_config);

    return EXIT_SUCCESS;
}

void cargar_config_struct_MEMORIA(t_config *archivo_config)
{
    config.puerto_escucha = config_get_string_value(archivo_config, "PUERTO_ESCUCHA");
    config.tam_memoria = config_get_string_value(archivo_config, "TAM_MEMORIA");
    config.tam_pagina = config_get_string_value(archivo_config, "TAM_PAGINA");
    config.path_instrucciones = config_get_string_value(archivo_config, "PATH_INSTRUCCIONES");
    config.retardo_respuesta = config_get_string_value(archivo_config, "RETARDO_RESPUESTA");
}

void *atender_cliente(void *cliente)
{
    int cliente_recibido = *(int *)cliente;
    while (1)
    {
        int cod_op = recibir_operacion(cliente_recibido); // bloqueante
        switch (cod_op)
        {
        case INICIAR_PROCESO: // memoria recibe de kernel el proceso, recibe el path y lo chequea!!
            t_list *path_recibido = recibir_paquete(cliente_recibido);
            crear_proceso(pid, path); // de donde saco el path y el pid xd????
            log_info(logger, "CREE EL PROCESO");
            break;
        case ELIMINAR_PROCESO: // memoria elimina el proceso, kernel le pasa el path o el pid
            eliminar_proceso(pid); // falta q obtenga el pid xd
            break;
        case LEER_PROCESO:
            // guarda BUFFER del paquete enviado
            t_sbuffer *buffer_lectura = cargar_buffer(cliente_recibido);

            // guarda datos del buffer (PID proceso + PC para buscar prox instruccion)
            uint32_t pid_proceso = buffer_read_uint32(buffer_lectura); // guardo PID del proceso del cual se quiere leer
            uint32_t pc_proceso = buffer_read_uint32(buffer_lectura);  // guardo PC para elegir la prox INSTRUCCION a ejecutar

            char *mensaje = (char *)malloc(128);
            sprintf(mensaje, "Recibi para leer del proceso %u, la prox INST desde PC %u", pid_proceso, pc_proceso);
            log_info(logger, "%s", mensaje);
            free(mensaje);

            // TODO: leer desde el proceso la próxima instruccion!!!!
            char nombreArchivo[256];
            snprintf(nombreArchivo, sizeof(nombreArchivo), "src/procesos/%d.txt", pid_proceso); // PROVISORIO!
            
            log_info(logger, nombreArchivo);

            int lineaActual = 0, lee_instruccion = 0;

            char *instruccion = NULL;
            size_t len = 0;
            FILE *script = fopen(nombreArchivo, "r");

            if (script == NULL)
                log_error(logger, "No se encontro ningun archivo con el nombre indicado...");
            else {
                while (getline(&instruccion, &len, script) != -1){
                    if (lineaActual == pc_proceso){
                        char *mensaje = (char *)malloc(128);
                        sprintf(mensaje, "INSTRUCCION LEIDA EN LINEA %d: %s", pc_proceso, instruccion);
                        log_info(logger, "%s", mensaje);
                        free(mensaje);
                        lee_instruccion = 1;
                        break;
                    }
                    lineaActual++;
                }
                fclose(script);
                
                if(lee_instruccion){
                    t_sbuffer *buffer_instruccion = buffer_create(
                        (uint32_t)strlen(instruccion) + sizeof(uint32_t)
                    );

                    buffer_add_string(buffer_instruccion, (uint32_t)strlen(instruccion), instruccion);
                    cargar_paquete(cliente_recibido, INSTRUCCION, buffer_instruccion);

                    buffer_destroy(buffer_lectura);
                } // else TODO: llega al final y CPU todavía no desalojó el proceso por EXIT (esto pasaría sólo si el proceso no termina con EXIT)
                free(instruccion);
            }
            break;
        case CONEXION:
            recibir_conexion(cliente_recibido);
            break;
        case PAQUETE:
            t_list *lista = recibir_paquete(cliente_recibido);
            log_info(logger, "Me llegaron los siguientes valores:\n");
            list_iterate(lista, (void *)iterator); // esto es un mapeo
            break;
        case -1:
            log_error(logger, "Cliente desconectado.");
            close(cliente_recibido); // cierro el socket accept del cliente
            free(cliente);           // libero el malloc reservado para el cliente
            pthread_exit(NULL);      // solo sale del hilo actual => deja de ejecutar la función atender_cliente que lo llamó
        default:
            log_warning(logger, "Operacion desconocida.");
            break;
        }
    }
}

////// funciones memoria

void inicializar_memoria(){

    memoria.num_frames = config.tam_memoria / config.tam_pagina;    //calculo del espacio de los frames
    memoria.espacio_usuario = malloc(config.tam_memoria);           
    memoria.tabla_paginas = malloc(memoria.num_frames * sizeof(int));

    for (int i = 0; i < memoria.num_frames; i++) { //esto es para recorrer la tabla e inicializarla toda en -1 
        memoria.tabla_paginas[i] = -1;
    } 

}

void crear_proceso(int pid, char* path){
    
    //validacion del archivo de instrucciones
    FILE *archivo = fopen(path, "r");
    if (!archivo) {
        log_error(logger, "el archivo del path '%s' no se abre :( ", path_instrucciones);
        return;
    }

    //creacion del pcb
    t_pcb *proceso = malloc(sizeof(t_pcb));
    proceso->estado = NEW;
    proceso->quantum = config.quantum;
    proceso->program_counter = 0;
    proceso->pid = pid;

    //variables aux:
    char* linea = NULL;
    int caracteres_leidos = 0, lenght = 0, pagina_actual = 0, offset = 0;

    while((caracteres_leidos = getline(&linea,&lenght,archivo)) != -1 ){//si la linea se lee correctamente, caracteres_leidos hace que el while sea true
        
        if (pagina_actual >= memoria.num_frames) { //check de si queda memoria suficiente para el proceso
            log_error(logger, "limite de memoria excedida para el proceso con pid %d", pid);
            fclose(archivo);
            free(linea);
            free(proceso);
            return;
        }

        if (offset + leidos > config.tam_pagina) { //check si la linea leida entra en en la pagina actual
            pagina_actual++;    //si así sucede, se pasa a la siguiente pagina
            offset = 0;         //y se reinicia el offset
        }

        int frame = encontrar_hueco();
        if (frame == -1) { //check si hay espacio en este momento para la asignacion de memoria al proceso
            log_error(logger, "no hay marcos libres para el proceso con pid %d", pid);
            fclose(archivo);
            free(linea);
            free(proceso);
            return;
        }

        /*
        falta asignar la memoria!!! y checkear si el pusheo a la lista esta bien.
        */

    }


    mqueue_push(monitor_NEW, proceso);

    log_iniciar_proceso(logger, proceso->pid);

    list_add(pcb_list, proceso);
    pid++;

    fclose(archivo);
    log_info(logger, "proceso %d cargado en memoria yay!!", pid);
}

/*void iniciar_proceso(char *path){                 la funcion que estaba en kernel

    t_pcb *proceso = malloc(sizeof(t_pcb));

    proceso->estado = NEW;
    proceso->quantum = config.quantum;
    proceso->program_counter = 0; // arranca en 0?
    proceso->pid = pid;

    mqueue_push(monitor_NEW, proceso);

    log_iniciar_proceso(logger, proceso->pid);

    list_add(pcb_list, proceso);
    pid++;
}*/

int encontrar_hueco() { //busco el hueco libre mas proximo!! 
    
    for (int i = 0; i < memoria.num_frames; i++) {
        if (memoria.tabla_paginas[i] == -1) {
            return i;
        }
    }
    return -1;
}

void eliminar_proceso(int pid) { //vuelvo a poner la tabla en -1!!! yay
    for (int i = 0; i < memoria.num_frames; i++) {
        if (memoria.tabla_paginas[i] == pid) {
            memoria.tabla_paginas[i] = -1;
        }
    }
    log_info(logger, "proceso %d eliminado", pid);
}