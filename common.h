#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

// Constantes Clave
#define RUTA_CLAVE_SERVIDOR "/tmp" // Ruta para generar la clave de la cola del servidor
#define ID_PROYECTO 'C'          // Carácter para generar la clave

// Límites del Sistema 
#define MAX_CLIENTES 50
#define MAX_SALAS 10
#define MAX_NOMBRE 50
#define MAX_TEXTO 256
#define RUTA_PERSISTENCIA "./historial/"

// Tipos de Mensajes (Enum en Español) 
typedef enum {
    // Solicitudes del Cliente
    TIPO_UNION_SALA = 1,
    TIPO_ABANDONAR_SALA,
    TIPO_MENSAJE,
    TIPO_LISTAR_SALAS,
    TIPO_LISTAR_USUARIOS,
    TIPO_CIERRE_CLIENTE,

    // Respuestas y Notificaciones del Servidor
    TIPO_RESPUESTA_EXITO = 101,
    TIPO_RESPUESTA_ERROR,
    TIPO_NOTIFICACION, // Para mensajes de chat y del sistema a la sala
} tipo_mensaje_t;

// Estructura del Mensaje
typedef struct {
    long mtype; // Tipo de mensaje (DEBE ser el primer campo)

    // Contenido del Mensaje
    int id_cola_cliente; // ID de la cola privada del cliente para respuestas
    char nombre_usuario[MAX_NOMBRE];
    char nombre_sala[MAX_NOMBRE];
    char texto[MAX_TEXTO];
} mensaje_t;

// Macro para calcular el tamaño útil del mensaje (sin mtype)
#define TAMANO_MENSAJE (sizeof(mensaje_t) - sizeof(long))

#endif // COMMON_H