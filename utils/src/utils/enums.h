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
	FIN_QUANTUM, // kernel - cpuInterrupt
	FINALIZAR_PROCESO,
	NO_FINALIZADO,
    DESALOJAR_PROCESO,
	RECIBI_INTERFAZ, // interfaz - kernel
	IO_GEN_SLEEP, // io
    IO_STDIN_READ, // io
    IO_STDOUT_WRITE, // io
	IO_FS_WRITE, // io
	IO_FS_READ, // io
    IO_FS_CREATE, // io
    IO_FS_DELETE, // io
    IO_FS_TRUNCATE, // io
	DESALOJAR, // KERNEL - CPU 
	CONTINUAR, // KERNEL - CPU
	PEDIR_FRAME, // CPU - MEMORIA
	TLB_MISS, //CPU - MEMORIA
	FRAME_SOLICIDATO // MEMORIA - CPU
}op_code;


#endif
