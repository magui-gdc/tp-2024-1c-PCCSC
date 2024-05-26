#include "buffer.h"

// Crea un buffer vacío de tamaño size y offset 0
t_sbuffer *buffer_create(uint32_t size) {
    t_sbuffer *buffer = malloc(sizeof(t_sbuffer));
    buffer->size = size;
    buffer->offset = 0;
    buffer->stream = malloc(size);
    return buffer;
}

// Libera la memoria asociada al buffer
void buffer_destroy(t_sbuffer *buffer) {
    free(buffer->stream);
    free(buffer);
}

// Agrega un stream al buffer en la posición actual y avanza el offset
void buffer_add(t_sbuffer *buffer, void *data, uint32_t size) {
    memcpy(buffer->stream + buffer->offset, data, size);
    buffer->offset += size;
}

// Guarda size bytes del principio del buffer en la dirección data y avanza el offset
void buffer_read(t_sbuffer *buffer, void *data, uint32_t size) {
    memcpy(data, buffer->stream + buffer->offset, size);
    buffer->offset += size;
}

// Agrega un uint32_t al buffer
void buffer_add_int(t_sbuffer *buffer, int data) {
    buffer_add(buffer, &data, sizeof(int));
}

uint32_t buffer_read_int(t_sbuffer *buffer) {
    uint32_t data;
    buffer_read(buffer, &data, sizeof(int));
    return data;
}

void buffer_add_uint32(t_sbuffer *buffer, uint32_t data) {
    buffer_add(buffer, &data, sizeof(uint32_t));
}

// Lee un uint32_t del buffer y avanza el offset
uint32_t buffer_read_uint32(t_sbuffer *buffer) {
    uint32_t data;
    buffer_read(buffer, &data, sizeof(uint32_t));
    return data;
}

// Agrega un uint8_t al buffer
void buffer_add_uint8(t_sbuffer *buffer, uint8_t data) {
    buffer_add(buffer, &data, sizeof(uint8_t));
}

// Lee un uint8_t del buffer y avanza el offset
uint8_t buffer_read_uint8(t_sbuffer *buffer) {
    uint8_t data;
    buffer_read(buffer, &data, sizeof(uint8_t));
    return data;
}

// Agrega string al buffer con un uint32_t adelante indicando su longitud
void buffer_add_string(t_sbuffer *buffer, uint32_t length, char *string) {
    buffer_add_uint32(buffer, length);
    buffer_add(buffer, string, length);
}

// Lee un string y su longitud del buffer y avanza el offset
char *buffer_read_string(t_sbuffer *buffer, uint32_t *length) {
    *length = buffer_read_uint32(buffer);
    char *string = malloc(*length + 1); // +1 para el carácter nulo
    buffer_read(buffer, string, *length);
    string[*length] = '\0'; // Añadir el carácter nulo al final
    return string;
}

// Agrega registros CPU al buffer
void buffer_add_registros(t_sbuffer *buffer, t_registros_cpu *registros) {
    buffer_add(buffer, &(registros->PC), sizeof(uint32_t));
    buffer_add(buffer, &(registros->AX), sizeof(uint8_t));
    buffer_add(buffer, &(registros->BX), sizeof(uint8_t));
    buffer_add(buffer, &(registros->CX), sizeof(uint8_t));
    buffer_add(buffer, &(registros->DX), sizeof(uint8_t));
    buffer_add(buffer, &(registros->EAX), sizeof(uint32_t));
    buffer_add(buffer, &(registros->EBX), sizeof(uint32_t));
    buffer_add(buffer, &(registros->ECX), sizeof(uint32_t));
    buffer_add(buffer, &(registros->EDX), sizeof(uint32_t));
    buffer_add(buffer, &(registros->SI), sizeof(uint32_t));
    buffer_add(buffer, &(registros->DI), sizeof(uint32_t));
}

// Lee registros CPU del buffer
void buffer_read_registros(t_sbuffer *buffer, t_registros_cpu *registros) {
    buffer_read(buffer, &(registros->PC), sizeof(uint32_t));
    buffer_read(buffer, &(registros->AX), sizeof(uint8_t));
    buffer_read(buffer, &(registros->BX), sizeof(uint8_t));
    buffer_read(buffer, &(registros->CX), sizeof(uint8_t));
    buffer_read(buffer, &(registros->DX), sizeof(uint8_t));
    buffer_read(buffer, &(registros->EAX), sizeof(uint32_t));
    buffer_read(buffer, &(registros->EBX), sizeof(uint32_t));
    buffer_read(buffer, &(registros->ECX), sizeof(uint32_t));
    buffer_read(buffer, &(registros->EDX), sizeof(uint32_t));
    buffer_read(buffer, &(registros->SI), sizeof(uint32_t));
    buffer_read(buffer, &(registros->DI), sizeof(uint32_t));
}

// Cargar paquete a partir de un BUFFER y un CODIGO DE OPERACION y enviarlo a un determinado SOCKET!
// La idea es que primero se cree el buffer con buffer_create; que luego se le carguen datos a ese buffet con las funciones buffer_add_*; y por último que se ejecute esta función
// La misma se encarga de liberar las estructuras dinámicas luego de enviar la información!
void cargar_paquete(int socket, op_code codigo_operacion, t_sbuffer *buffer){
    // --------- se carga el paquete 
    t_spaquete* paquete = malloc(sizeof(t_spaquete));

    paquete->codigo_operacion = codigo_operacion; 
    paquete->buffer = buffer; 

    // --------- se arma el stream a enviar
    void* a_enviar = malloc(buffer->size + sizeof(int) + sizeof(uint32_t)); // se reservar espacio para el buffer, para el código de operación y para el tamaño del buffer
    int offset = 0;

    memcpy(a_enviar + offset, &(paquete->codigo_operacion), sizeof(int)); // la función recibir operación es el primer valor que toma!
    offset += sizeof(int);
    memcpy(a_enviar + offset, &(paquete->buffer->size), sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(a_enviar + offset, paquete->buffer->stream, paquete->buffer->size);

    // --------- se envía el paquete 
    send(socket, a_enviar, buffer->size + sizeof(int) + sizeof(uint32_t), 0);

    // --------- se libera memoria
    free(a_enviar);
    buffer_destroy(buffer);
    free(paquete);
}


t_sbuffer* cargar_buffer(int socket) {
    t_sbuffer *buffer = malloc(sizeof(t_sbuffer));
    if (buffer == NULL) {
        perror("Error al asignar memoria para buffer");
        return NULL;
    }

    buffer->offset = 0;

    recv(socket, &(buffer->size), sizeof(uint32_t), MSG_WAITALL);
    buffer->stream = malloc(buffer->size);
    if (buffer->stream == NULL) {
        perror("Error al asignar memoria para buffer->stream");
        free(buffer);
        return NULL;
    }

    recv(socket, buffer->stream, buffer->size, MSG_WAITALL);

    return buffer;
}
