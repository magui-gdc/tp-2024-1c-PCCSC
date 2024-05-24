#ifndef ENUMS_H
#define ENUMS_H

typedef enum
{
	CONEXION, // cliente - servidor
	PAQUETE,
	INICIAR_PROCESO, // kernel - memoria
	ELIMINAR_PROCESO, // kernel - memoria
	INSTRUCCION, // memoria - cpu
	EJECUTAR_PROCESO, // kernel - cpu
	INTERRUPCION, // kernel - cpu
	LEER_PROCESO, // cpu - memoria
	EXIT_PROCESO, // cpu - kernel
	WAIT_RECURSO, // cpu - kernel
	SIGNAL_RECURSO, // cpu - kernel 
	FIN_QUANTUM // kernel - cpuInterrupt
}op_code;

#endif
