#include "instrucciones.h"

// TODO: JNZ e IO_GEN_SLEEP

int contar_digitos(int num) {
    if (num == 0) return 1; 
    if (num < 0) num = -num;
    return (int)log10(num) + 1;
}

void intToCadena(int num, char* array, int longitud){
    if (num < 0) num = -num; // por ser unsigned los tipos de los registros
    for (int i = longitud - 1; i >= 0; i--) {
        array[i] = (num % 10) + '0';
        num /= 10;
    }
    array[longitud] = '\0';
}

tiposDeDato obtenerTipo(char* registro){
    if (strcmp(registro, "AX") == 0 || strcmp(registro, "BX") == 0 || 
        strcmp(registro, "CX") == 0 || strcmp(registro, "DX") == 0) {
        return _UINT8;
    } else if (strcmp(registro, "EAX") == 0 || strcmp(registro, "EBX") == 0 || 
               strcmp(registro, "ECX") == 0 || strcmp(registro, "EDX") == 0 || 
               strcmp(registro, "SI") == 0 || strcmp(registro, "DI") == 0 || 
               strcmp(registro, "PC") == 0) {
        return _UINT32;
    } else {
        // Manejar el caso de registro no válido
        perror("registro no valido");
        exit(EXIT_FAILURE);
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
    
    // inicializo para que no haya problemas!
    uint8_t cast_8bits;
    uint32_t cast_32bits;
    uint8_t* dir_8bits = NULL;  // Inicializar a NULL
    uint32_t* dir_32bits = NULL;

    obtenerDireccionMemoria(registro, &dir_8bits, &dir_32bits);

    switch (obtenerTipo(registro)){
        case _UINT8:
            cast_8bits = (uint8_t)valor_numerico;
            *dir_8bits = cast_8bits;
            break;
        case _UINT32:
            cast_32bits = (uint32_t)valor_numerico;
            *dir_32bits = cast_32bits;
            break;
        default:
            break;
    }

    // NO hay asignación dinámica de memoria siempre, liberar luego de llamar a la función según cada caso
    /*
    free(*dir_8bits);
    free(dir_8bits);
    free(*dir_32bits);
    free(dir_32bits);
    */
}

void MOV_IN (char* registroDato, char* registroDireccion){

    if(obtenerTipo(registroDato) == obtenerTipo(registroDireccion)){
    
        uint8_t dato_8bits;
        uint32_t dato_32bits;
        uint8_t* dir_8bits = NULL;
        uint32_t* dir_32bits = NULL;

        int longitud;
        char* array;

        obtenerDireccionMemoria(registroDireccion, &dir_8bits, &dir_32bits);

        switch (obtenerTipo(registroDireccion)){
            case _UINT8:
                dato_8bits = *dir_8bits;
                longitud = contar_digitos((int)dato_8bits);
                array = (char*)malloc((longitud + 1) * sizeof(char));
                intToCadena(dato_8bits, array, longitud);
                set(registroDato, array);
                break;
            case _UINT32:
                dato_32bits = *dir_32bits;
                longitud = contar_digitos((int)dato_32bits);
                array = (char*)malloc((longitud + 1) * sizeof(char));
                intToCadena(dato_32bits, array, longitud);
                set(registroDato, array);
                break;
            default:
                break;
        }
        if(array != NULL){
            free(array);
        }

    }
}// Lee el valor de memoria correspondiente a la Dirección Lógica que se encuentra en el Registro Dirección y lo almacena en el Registro Datos.

void MOV_OUT (char* registroDireccion, char* registroDato){
    MOV_IN(registroDireccion, registroDato);
}

void SUM (char* registroDireccion, char* registroOrigen, t_log* logger){
    char *prueba = (char *)malloc(128);
    sprintf(prueba, "%s: %d, %s: %d", registroDireccion, obtenerTipo(registroDireccion), registroOrigen, obtenerTipo(registroOrigen));
    log_info(logger, "%s", prueba);
    free(prueba);
    if(obtenerTipo(registroOrigen) == obtenerTipo(registroDireccion)){
        uint8_t* dir_8bits1;
        uint32_t* dir_32bits1;
        uint8_t* dir_8bits2;
        uint32_t* dir_32bits2;

        int suma;
        int longitudSuma;
        uint32_t valorOrigen;
        uint32_t valorDestino;

        obtenerDireccionMemoria(registroOrigen, &dir_8bits1, &dir_32bits1);
        obtenerDireccionMemoria(registroDireccion, &dir_8bits2, &dir_32bits2);

        switch (obtenerTipo(registroOrigen))
        {
            case _UINT8:
                valorOrigen = *dir_8bits1;
                valorDestino = *dir_8bits2;            
                break;
            case _UINT32:
                valorOrigen = *dir_32bits1;
                valorDestino = *dir_32bits2;
                break;
            default:
                break;
        }

        suma = valorDestino + valorOrigen;
        longitudSuma = contar_digitos(suma);
        char* arraySuma = (char*)malloc((longitudSuma + 1));

        intToCadena(suma, arraySuma, longitudSuma);

        char *mens = (char *)malloc(128);
        sprintf(mens, "suma %d, array %s, longitud %d", suma, arraySuma, longitudSuma);
        log_info(logger, "%s", mens);
        free(mens);

        set(registroDireccion, arraySuma);
        free(arraySuma);

    }    
}

void SUB (char* registroDireccion, char* registroOrigen){
    if(obtenerTipo(registroOrigen) == obtenerTipo(registroDireccion)){
            
        uint8_t* dir_8bits1;
        uint32_t* dir_32bits1;
        uint8_t* dir_8bits2;
        uint32_t* dir_32bits2;

        int resta;
        int longitudResta;
        uint32_t valorOrigen;
        uint32_t valorDestino;

        obtenerDireccionMemoria(registroOrigen, &dir_8bits1, &dir_32bits1);
        obtenerDireccionMemoria(registroDireccion, &dir_8bits2, &dir_32bits2);

        switch (obtenerTipo(registroOrigen)){
            case _UINT8:
                valorOrigen = *dir_8bits1;
                valorDestino = *dir_8bits2;
                break;
            case _UINT32:
                valorOrigen = *dir_32bits1;
                valorDestino = *dir_32bits2;
                break;
            default:
                break;
        }

        resta = valorDestino - valorOrigen;
        longitudResta = contar_digitos(resta);
        char* arrayResta = (char*)malloc((longitudResta + 1));

        intToCadena(resta, arrayResta, longitudResta);

        set(registroDireccion, arrayResta);
        free(arrayResta);
    }    
}

void jnz(char* registro, char* numeroInstruccion) {
    uint8_t* dir_8bits;
    uint32_t* dir_32bits;
    uint32_t valorRegistro;
    int valor_numerico = atoi(numeroInstruccion);

    obtenerDireccionMemoria(registro, &dir_8bits, &dir_32bits);

    switch (obtenerTipo(registro)) {
        case _UINT8:
            valorRegistro = *dir_8bits;
            break;
        case _UINT32:
            valorRegistro = *dir_32bits;
            break;
        default:
            break;
    }

    if (valorRegistro != 0)
        registros.PC = (uint32_t)valor_numerico;
}