#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<commons/config.h>

typedef struct {
    uint32_t PC;
    // registros numéricos de propósito general
    uint8_t AX;
    uint8_t BX;
    uint8_t CX;
    uint8_t DX;
    uint32_t EAX;
    uint32_t EBX;
    uint32_t ECX;
    uint32_t EDX;
    // registros con direcciones lógicas 
    uint32_t SI; // de memoria de origen desde donde se va a copiar un string
    uint32_t DI; // de memoria de destino a donde se a a copiar un string
} t_registros_cpu;

t_config* iniciar_config(char*);