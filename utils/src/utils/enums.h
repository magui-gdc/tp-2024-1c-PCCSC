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
	WAIT, // Instrucción WAIT cpu - kernel
	SIGNAL, // Instruccion SIGNAL cpu - kernel
	INSTRUCCION_EXIT, // Instrucción EXIT cpu - kernel
	INTERRUPCION, // kernel - cpu
	LEER_PROCESO // cpu - memoria
}op_code;

#endif
