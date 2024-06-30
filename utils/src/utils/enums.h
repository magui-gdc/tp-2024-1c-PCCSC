#ifndef ENUMS_H
#define ENUMS_H

typedef enum{
	CONEXION, // cliente - servidor
	DATOS_MEMORIA, // cpu - memoria
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
	IO_GEN_SLEEP, // kernel - io (16)
	IO_MEMORY_DONE, // memoria - io
    IO_STDIN_READ, // kernel - io
    IO_STDOUT_WRITE, // kernel - io
	IO_FS_WRITE, // kernel - io
	IO_FS_READ, // kernel - io
    IO_FS_CREATE, // kernel - io
    IO_FS_DELETE, // kernel - io
    IO_FS_TRUNCATE, // kernel - io
	IO_LIBERAR, // io - kernel (luego de terminar de ejecutar la instrucción IO exitosamente)
	DESALOJAR, // KERNEL - CPU 
	CONTINUAR, // KERNEL - CPU
	TLB_MISS, //CPU - MEMORIA
	MARCO_SOLICITADO, // MEMORIA - CPU
	MOV_IN, 
	MOV_OUT,
	RESIZE,
	OUT_OF_MEMORY,
	COPY_STRING,
	PETICION_ESCRITURA,  // IO/CPU - MEMORIA
	PETICION_LECTURA // IO/CPU - MEMORIA
}op_code;


#endif
