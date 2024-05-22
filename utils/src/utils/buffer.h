#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "config.h"
#include "enums.h"

typedef struct {
    uint32_t size; // Tamaño del payload
    uint32_t offset; // Desplazamiento dentro del payload
    void *stream; // Payload
} t_sbuffer;

typedef struct {
    op_code codigo_operacion;
    t_sbuffer* buffer;
} t_spaquete;

// Crea un buffer vacío de tamaño size y offset 0
t_sbuffer *buffer_create(uint32_t size);

// Libera la memoria asociada al buffer
void buffer_destroy(t_sbuffer *buffer);

// Agrega un stream al buffer en la posición actual y avanza el offset
void buffer_add(t_sbuffer *buffer, void *data, uint32_t size);

// Guarda size bytes del principio del buffer en la dirección data y avanza el offset
void buffer_read(t_sbuffer *buffer, void *data, uint32_t size);

// Agrega un uint32_t al buffer
void buffer_add_uint32(t_sbuffer *buffer, uint32_t data);

// Lee un uint32_t del buffer y avanza el offset
uint32_t buffer_read_uint32(t_sbuffer *buffer);

// Agrega un uint8_t al buffer
void buffer_add_uint8(t_sbuffer *buffer, uint8_t data);

// Lee un uint8_t del buffer y avanza el offset
uint8_t buffer_read_uint8(t_sbuffer *buffer);

// Agrega string al buffer con un uint32_t adelante indicando su longitud
void buffer_add_string(t_sbuffer *buffer, uint32_t length, char *string);

// Lee un string y su longitud del buffer y avanza el offset
char *buffer_read_string(t_sbuffer *buffer, uint32_t *length);

// Agrega registros CPU al buffer
void buffer_add_registros(t_sbuffer *buffer, t_registros_cpu *registros);

// Lee registros CPU del buffer
void buffer_read_registros(t_sbuffer *buffer, t_registros_cpu *registros);

// Cargar paquete a partir de un BUFFER y un CODIGO DE OPERACION y enviarlo a un determinado SOCKET!
// La idea es que primero se cree el buffer con buffer_create; que luego se le carguen datos a ese buffet con las funciones buffer_add_*; y por último que se ejecute esta función
// La misma se encarga de liberar las estructuras dinámicas luego de enviar la información!
void cargar_paquete(int socket, op_code codigo_operacion, t_sbuffer *buffer);
