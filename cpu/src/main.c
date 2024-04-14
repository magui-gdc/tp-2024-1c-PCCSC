#include <stdlib.h>
#include <stdio.h>
#include <utils/hello.h>

int main(int argc, char* argv[]) {
    
    int conexion_memoria;
    t_config* archivo_config = iniciar_config_cpu("cpu.config");    
    t_log* logger = log_create("cpu.log", "CPU", 1, LOG_LEVEL_DEBUG);

    cargar_config_struct_CPU(archivo_config);

    decir_hola("CPU");

    //Conexion con Memoria
    conexion_memoria = crear_conexion(config.ip_memoria, config.puerto_memoria);
    enviar_conexion("CPU", conexion_memoria);


    //Limpieza
    log_destroy(logger);
	config_destroy(config);
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
