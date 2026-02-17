#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define LOCALHOST "127.0.0.1"
#define DEFAULT_PORT 6677
#define BUFFER_SIZE 2048
#define NAME_LEN 32
#define ROOM_LEN 32

// tipos de mensaje para el protocolo
typedef enum {
    MSG_TYPE_DH_PUBLIC = 0x01,   // intercambio de claves DH
    MSG_TYPE_CHAT = 0x02,        // mensaje normal
    MSG_TYPE_COMMAND = 0x03,     // comando
    MSG_TYPE_SERVER = 0x04       // mensaje del server
} message_type_t;

// estructura de cabecera para todos los mensajes
typedef struct {
    uint8_t type;           // tipo de mensaje
    uint32_t length;        // longitud del payload
} __attribute__((packed)) message_header_t;

#endif
