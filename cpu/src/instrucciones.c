#include "instrucciones.h"

// TODO: JNZ e IO_GEN_SLEEP

int contar_digitos(int num) {
    if (num == 0) return 1; 
    if (num < 0) num = -num;
    return (int)log10(num) + 1;
}

char* intToCadena(int num, char* array, int longitud){
    if (num < 0) num = -num;
    for (int i = longitud - 1; i >= 0; i--) {
        array[i] = (num % 10) + '0';
        num /= 10;
    }
    //TODO: falta return
}

tiposDeDato obtenerTipo(char* registro){
    if(strcmp(registro, "AX") == 0){
        return _UINT8;
    }
    if(strcmp(registro, "BX") == 0){
        return _UINT8;
    }
    if(strcmp(registro, "CX") == 0){
        return _UINT8;
    }
    if(strcmp(registro, "DX") == 0){
        return _UINT8;
    }
    if(strcmp(registro, "EAX") == 0){
        return _UINT32;
    }
    if(strcmp(registro, "EBX") == 0){
        return _UINT32;
    }
    if(strcmp(registro, "ECX") == 0){
        return _UINT32;
    }
    if(strcmp(registro, "EDX") == 0){
        return _UINT32;
    }
    if(strcmp(registro, "SI") == 0){
        return _UINT32;
    }
    if(strcmp(registro, "DI") == 0){
        return _UINT32;
    }
    if(strcmp(registro, "PC") == 0){
        return _UINT32;
    } 
}

void obtenerDireccionMemoria(char* registro, uint8_t** dato_8bits, uint32_t** dato_32bits){
    *dato_8bits = NULL;
    *dato_32bits = NULL;

    if(strcmp(registro, "AX") == 0){
        *dato_8bits = &(registros.AX);
        /*Este es un puntero doble a uint8_t. 
        En la función obtenerDireccionMemoria, dato_8bits es un puntero doble que apunta
         a un puntero uint8_t. Esto significa que dato_8bits puede almacenar la dirección
         de memoria de un puntero uint8_t.*/
    }
    if(strcmp(registro, "BX") == 0){
        *dato_8bits = &(registros.BX);
    }
    if(strcmp(registro, "CX") == 0){
        *dato_8bits = &(registros.CX);
    }
    if(strcmp(registro, "DX") == 0){
        *dato_8bits = &(registros.DX);
    }
    if(strcmp(registro, "EAX") == 0){
        *dato_32bits = &(registros.EAX);
    }
    if(strcmp(registro, "EBX") == 0){
        *dato_32bits = &(registros.EBX);
    }
    if(strcmp(registro, "ECX") == 0){
       *dato_32bits = &(registros.ECX);
    }
    if(strcmp(registro, "EDX") == 0){
        *dato_32bits = &(registros.EDX);
    }
    if(strcmp(registro, "SI") == 0){
        *dato_32bits = &(registros.SI);
    }
    if(strcmp(registro, "DI") == 0){
        *dato_32bits = &(registros.DI);
    }
    if(strcmp(registro, "PC") == 0){
        *dato_32bits = &(registros.PC);
    } 
}

void set(char* registro, char* valor){
    int valor_numerico = atoi(valor);
    
    uint8_t cast_8bits;
    uint32_t cast_32bits;
    uint8_t** dir_8bits; // problemas con la inicialización, VER.
    uint32_t** dir_32bits;

    obtenerDireccionMemoria(registro, dir_8bits, dir_32bits);

    switch (obtenerTipo(registro)){
        case _UINT8:
            cast_8bits = (uint8_t)valor_numerico;
            **dir_8bits = cast_8bits;
            break;
        case _UINT32:
            cast_32bits = (uint32_t)valor_numerico;
            **dir_32bits = cast_32bits;
            break;
        default:
            break;
    }

    free(*dir_8bits);
    free(dir_8bits);
    free(*dir_32bits);
    free(dir_32bits);
}

void MOV_IN (char* registroDato, char* registroDireccion){

    if(obtenerTipo(registroDato) == obtenerTipo(registroDireccion)){
    
        uint8_t dato_8bits;
        uint32_t dato_32bits;
        
        uint8_t** dir_8bits;
        uint32_t** dir_32bits;

        int longitud;
        
        char* array;

        obtenerDireccionMemoria(registroDireccion, dir_8bits, dir_32bits);
        

        switch (obtenerTipo(registroDireccion))
        {
            case _UINT8:
                dato_8bits = **dir_8bits;
                longitud = contar_digitos((int)dato_8bits);
                array = (char*)malloc((longitud + 1) * sizeof(char));
                intToCadena(dato_8bits, array, longitud);
                set(registroDato, array);
                break;
            case _UINT32:
                dato_32bits = **dir_32bits;
                longitud = contar_digitos((int)dato_32bits);
                array = (char*)malloc((longitud + 1) * sizeof(char));
                intToCadena(dato_32bits, array, longitud);
                set(registroDato, array);
                break;
            default:
                break;
        }

    }
}// Lee el valor de memoria correspondiente a la Dirección Lógica que se encuentra en el Registro Dirección y lo almacena en el Registro Datos.

void MOV_OUT (char* registroDireccion, char* registroDato){
    MOV_IN(registroDireccion, registroDato);
}

void SUM (char* registroDireccion, char* registroOrigen){
    if(obtenerTipo(registroOrigen) == obtenerTipo(registroDireccion)){
            
        uint8_t** dir_8bits1;
        uint32_t** dir_32bits1;
        uint8_t** dir_8bits2;
        uint32_t** dir_32bits2;

        int suma;
        int longitudSuma;
        int valorOrigen;
        int valorDestino;

        obtenerDireccionMemoria(registroOrigen, dir_8bits1, dir_32bits1);
        obtenerDireccionMemoria(registroDireccion, dir_8bits2, dir_32bits2);

        switch (obtenerTipo(registroOrigen))
        {
            case _UINT8:
                valorOrigen = **dir_8bits1;
                valorDestino = **dir_8bits2;
                break;
            case _UINT32:
                valorOrigen = **dir_32bits1;
                valorDestino = **dir_32bits2;
                break;
            default:
                break;
        }

        suma = valorDestino + valorDestino;
        longitudSuma = contar_digitos(suma);
        char* arraySuma = (char*)malloc((longitudSuma + 1) * sizeof(char));

        intToCadena(suma, arraySuma, longitudSuma);

        set(registroDireccion, arraySuma);

    }    
}

void SUB (char* registroDireccion, char* registroOrigen){
    if(obtenerTipo(registroOrigen) == obtenerTipo(registroDireccion)){
            
        uint8_t** dir_8bits1;
        uint32_t** dir_32bits1;
        uint8_t** dir_8bits2;
        uint32_t** dir_32bits2;

        int resta;
        int longitudResta;
        int valorOrigen;
        int valorDestino;

        obtenerDireccionMemoria(registroOrigen, dir_8bits1, dir_32bits1);
        obtenerDireccionMemoria(registroDireccion, dir_8bits2, dir_32bits2);

        switch (obtenerTipo(registroOrigen))
        {
            case _UINT8:
                valorOrigen = **dir_8bits1;
                valorDestino = **dir_8bits2;
                break;
            case _UINT32:
                valorOrigen = **dir_32bits1;
                valorDestino = **dir_32bits2;
                break;
            default:
                break;
        }

        resta = valorDestino - valorDestino;
        longitudResta = contar_digitos(resta);
        char* arrayResta = (char*)malloc((longitudResta + 1) * sizeof(char));

        intToCadena(resta, arrayResta, longitudResta);

        set(registroDireccion, arrayResta);

    }    
}