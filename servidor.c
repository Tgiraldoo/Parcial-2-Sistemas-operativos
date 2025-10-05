#include "common.h"
#include <sys/stat.h> // Para mkdir

// Estructuras de Datos del Servidor 
typedef struct {
    int id_cola;
    char nombre_usuario[MAX_NOMBRE];
    int indice_sala; // Índice a la sala en la que está el cliente (-1 si no está)
} cliente_t;

typedef struct {
    char nombre[MAX_NOMBRE];
    int indices_clientes[MAX_CLIENTES]; // Array de índices al array global de clientes
    int num_clientes;
} sala_t;

// Variables Globales del Servidor
static cliente_t clientes[MAX_CLIENTES];
static sala_t salas[MAX_SALAS];
static int num_clientes_activos = 0;
static int num_salas_activas = 0;
static int id_cola_servidor = -1;

//  Prototipos de Funciones (Modularidad)
void finalizar_servidor(int signum);
void gestionar_union_sala(mensaje_t* msg);
void gestionar_abandonar_sala(mensaje_t* msg, int notificar_cliente);
void gestionar_mensaje_sala(mensaje_t* msg);
void gestionar_listar_salas(mensaje_t* msg);
void gestionar_listar_usuarios(mensaje_t* msg);
void gestionar_cierre_cliente(mensaje_t* msg);
int buscar_o_crear_sala(const char* nombre_sala);
int buscar_cliente_por_id_cola(int id_cola);
void difundir_notificacion(int indice_sala, const char* texto, int id_cola_excluida);
void enviar_respuesta_a_cliente(int id_cola_cliente, tipo_mensaje_t tipo, const char* texto);
void registrar_mensaje_en_log(const char* nombre_sala, const char* nombre_usuario, const char* texto);

/**
 * @brief Función principal del servidor.
 */
int main() {
    printf("Iniciando servidor de chat...\n");
    signal(SIGINT, finalizar_servidor); // Manejar Ctrl+C para limpieza

    // Crear directorio para persistencia
    mkdir(RUTA_PERSISTENCIA, 0777);

    key_t clave_servidor = ftok(RUTA_CLAVE_SERVIDOR, ID_PROYECTO);
    if (clave_servidor == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    id_cola_servidor = msgget(clave_servidor, IPC_CREAT | 0666);
    if (id_cola_servidor == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }
    printf("Servidor escuchando en la cola con ID: %d\n", id_cola_servidor);

    // Bucle principal para recibir y procesar mensajes
    mensaje_t msg_recibido;
    while (1) {
        if (msgrcv(id_cola_servidor, &msg_recibido, TAMANO_MENSAJE, 0, 0) == -1) {
            if (errno == EINTR) continue; // Interrumpido por señal, continuar
            perror("msgrcv");
            continue;
        }

        switch (msg_recibido.mtype) {
            case TIPO_UNION_SALA:       gestionar_union_sala(&msg_recibido);       break;
            case TIPO_ABANDONAR_SALA:   gestionar_abandonar_sala(&msg_recibido, 1);break;
            case TIPO_MENSAJE:          gestionar_mensaje_sala(&msg_recibido);     break;
            case TIPO_LISTAR_SALAS:     gestionar_listar_salas(&msg_recibido);     break;
            case TIPO_LISTAR_USUARIOS:  gestionar_listar_usuarios(&msg_recibido);  break;
            case TIPO_CIERRE_CLIENTE:   gestionar_cierre_cliente(&msg_recibido);   break;
            default: fprintf(stderr, " Mensaje de tipo desconocido: %ld\n", msg_recibido.mtype);
        }
    }
    return 0;
}

/**
 * @brief Maneja la solicitud de un cliente para unirse a una sala.
 */
void gestionar_union_sala(mensaje_t* msg) {
    int indice_cliente = buscar_cliente_por_id_cola(msg->id_cola_cliente);
    if (indice_cliente == -1) { // Cliente nuevo
        if (num_clientes_activos >= MAX_CLIENTES) {
            enviar_respuesta_a_cliente(msg->id_cola_cliente, TIPO_RESPUESTA_ERROR, "El servidor está lleno.");
            return;
        }
        indice_cliente = num_clientes_activos++;
        clientes[indice_cliente].id_cola = msg->id_cola_cliente;
        strncpy(clientes[indice_cliente].nombre_usuario, msg->nombre_usuario, MAX_NOMBRE);
        clientes[indice_cliente].indice_sala = -1;
        printf("ℹ Nuevo cliente conectado: %s (ID Cola: %d)\n", msg->nombre_usuario, msg->id_cola_cliente);
    }
    
    // Si el cliente ya estaba en una sala, lo sacamos de la anterior
    if (clientes[indice_cliente].indice_sala != -1) {
        gestionar_abandonar_sala(msg, 0); 
    }

    int indice_sala = buscar_o_crear_sala(msg->nombre_sala);
    if (indice_sala == -1) {
        enviar_respuesta_a_cliente(msg->id_cola_cliente, TIPO_RESPUESTA_ERROR, "No se pudo crear la sala (límite alcanzado).");
        return;
    }

    sala_t* sala = &salas[indice_sala];
    if (sala->num_clientes >= MAX_CLIENTES) {
        enviar_respuesta_a_cliente(msg->id_cola_cliente, TIPO_RESPUESTA_ERROR, "La sala está llena.");
        return;
    }
    
    // Añadir cliente a la sala
    sala->indices_clientes[sala->num_clientes++] = indice_cliente;
    clientes[indice_cliente].indice_sala = indice_sala;

    char texto_buffer[MAX_TEXTO];
    snprintf(texto_buffer, sizeof(texto_buffer), "Te has unido a la sala '%s'.", msg->nombre_sala);
    enviar_respuesta_a_cliente(msg->id_cola_cliente, TIPO_RESPUESTA_EXITO, texto_buffer);
    
    snprintf(texto_buffer, sizeof(texto_buffer), "[SISTEMA] %s se ha unido a la sala.", msg->nombre_usuario);
    difundir_notificacion(indice_sala, texto_buffer, msg->id_cola_cliente);
    registrar_mensaje_en_log(sala->nombre, "SISTEMA", texto_buffer);

    printf(" Cliente %s se unió a la sala %s\n", msg->nombre_usuario, msg->nombre_sala);
}


/**
 * @brief Maneja la salida de un cliente de una sala.
 */
void gestionar_abandonar_sala(mensaje_t* msg, int notificar_cliente) {
    int indice_cliente = buscar_cliente_por_id_cola(msg->id_cola_cliente);
    if (indice_cliente == -1 || clientes[indice_cliente].indice_sala == -1) return;

    int indice_sala = clientes[indice_cliente].indice_sala;
    sala_t* sala = &salas[indice_sala];

    // Eliminar al cliente de la sala
    for (int i = 0; i < sala->num_clientes; i++) {
        if (sala->indices_clientes[i] == indice_cliente) {
            
            sala->indices_clientes[i] = sala->indices_clientes[sala->num_clientes - 1];
            sala->num_clientes--;
            break;
        }
    }
    clientes[indice_cliente].indice_sala = -1;

    // Notificar al cliente (si es necesario) y a los demás
    if (notificar_cliente) {
        enviar_respuesta_a_cliente(msg->id_cola_cliente, TIPO_RESPUESTA_EXITO, "Has abandonado la sala.");
    }
    
    char texto_buffer[MAX_TEXTO];
    snprintf(texto_buffer, sizeof(texto_buffer), "[SISTEMA] %s ha abandonado la sala.", clientes[indice_cliente].nombre_usuario);
    difundir_notificacion(indice_sala, texto_buffer, -1); // -1 para no excluir a nadie
    registrar_mensaje_en_log(sala->nombre, "SISTEMA", texto_buffer);

    printf("ℹ Cliente %s ha salido de la sala %s\n", clientes[indice_cliente].nombre_usuario, sala->nombre);
}


/**
 * @brief Maneja un mensaje de chat y lo reenvía a la sala.
 */
void gestionar_mensaje_sala(mensaje_t* msg) {
    int indice_cliente = buscar_cliente_por_id_cola(msg->id_cola_cliente);
    if (indice_cliente == -1 || clientes[indice_cliente].indice_sala == -1) {
        enviar_respuesta_a_cliente(msg->id_cola_cliente, TIPO_RESPUESTA_ERROR, "No estás en una sala.");
        return;
    }

    int indice_sala = clientes[indice_cliente].indice_sala;
    char texto_buffer[MAX_TEXTO + MAX_NOMBRE + 5];
    snprintf(texto_buffer, sizeof(texto_buffer), "[%s]: %s", msg->nombre_usuario, msg->texto);

    difundir_notificacion(indice_sala, texto_buffer, msg->id_cola_cliente);
    registrar_mensaje_en_log(salas[indice_sala].nombre, msg->nombre_usuario, msg->texto);
}


/**
 * @brief Envía la lista de salas disponibles al cliente.
 */
void gestionar_listar_salas(mensaje_t* msg) {
    
    char buffer[MAX_TEXTO * 2];
    char* ptr = buffer;
    size_t remaining_size = sizeof(buffer);
    int written;

    written = snprintf(ptr, remaining_size, "Salas disponibles:\n");
    ptr += written;
    remaining_size -= written;

    if (num_salas_activas == 0) {
        snprintf(ptr, remaining_size, " - No hay salas activas.\n");
    } else {
        for (int i = 0; i < num_salas_activas; i++) {
            written = snprintf(ptr, remaining_size, " - %s (%d/%d)\n", salas[i].nombre, salas[i].num_clientes, MAX_CLIENTES);
            
            
            if ((size_t)written >= remaining_size) {
                break; 
            }
            ptr += written;
            remaining_size -= written;
        }
    }
    enviar_respuesta_a_cliente(msg->id_cola_cliente, TIPO_RESPUESTA_EXITO, buffer);
}


/**
 * @brief Envía la lista de usuarios en la sala actual del cliente.
 */
void gestionar_listar_usuarios(mensaje_t* msg) {
    int indice_cliente = buscar_cliente_por_id_cola(msg->id_cola_cliente);
    if (indice_cliente == -1 || clientes[indice_cliente].indice_sala == -1) {
        enviar_respuesta_a_cliente(msg->id_cola_cliente, TIPO_RESPUESTA_ERROR, "No estás en una sala.");
        return;
    }
    
    int indice_sala = clientes[indice_cliente].indice_sala;
    sala_t* sala = &salas[indice_sala];

    
    char buffer[MAX_TEXTO * 2];
    char* ptr = buffer;
    size_t remaining_size = sizeof(buffer);
    int written;

    written = snprintf(ptr, remaining_size, "Usuarios en '%s':\n", sala->nombre);
    ptr += written;
    remaining_size -= written;

    for (int i = 0; i < sala->num_clientes; i++) {
        written = snprintf(ptr, remaining_size, " - %s\n", clientes[sala->indices_clientes[i]].nombre_usuario);
        
        
        if ((size_t)written >= remaining_size) {
            break; // Buffer lleno.
        }
        ptr += written;
        remaining_size -= written;
    }
    enviar_respuesta_a_cliente(msg->id_cola_cliente, TIPO_RESPUESTA_EXITO, buffer);
}


/**
 * @brief Gestiona la desconexión de un cliente.
 */
void gestionar_cierre_cliente(mensaje_t* msg) {
    int indice_cliente = buscar_cliente_por_id_cola(msg->id_cola_cliente);
    if (indice_cliente == -1) return;

    // Si el cliente estaba en una sala, se gestiona su salida de la misma.
    if (clientes[indice_cliente].indice_sala != -1) {
        // Creamos un mensaje temporal para la función, ya que el original no es para abandonar.
        mensaje_t msg_leave = *msg; 
        strncpy(msg_leave.nombre_sala, salas[clientes[indice_cliente].indice_sala].nombre, MAX_NOMBRE);
        gestionar_abandonar_sala(&msg_leave, 0); // 0 = No notificar al cliente que ya se está cerrando.
    }

    printf(" Cliente %s (ID Cola: %d) se ha desconectado.\n", clientes[indice_cliente].nombre_usuario, msg->id_cola_cliente);

    
    // Se mueve el último cliente a la posición del que se desconectó.
    clientes[indice_cliente] = clientes[num_clientes_activos - 1];
    num_clientes_activos--;
}


/**
 * @brief Busca una sala por nombre, si no existe, la crea.
 * @return El índice de la sala o -1 si hubo un error.
 */
int buscar_o_crear_sala(const char* nombre_sala) {
    for (int i = 0; i < num_salas_activas; i++) {
        if (strcmp(salas[i].nombre, nombre_sala) == 0) {
            return i;
        }
    }
    if (num_salas_activas >= MAX_SALAS) return -1;

    int nuevo_indice = num_salas_activas++;
    strncpy(salas[nuevo_indice].nombre, nombre_sala, MAX_NOMBRE);
    salas[nuevo_indice].num_clientes = 0;
    printf(" Nueva sala creada: %s\n", nombre_sala);
    return nuevo_indice;
}


/**
 * @brief Busca un cliente por su ID de cola.
 * @return El índice del cliente o -1 si no se encuentra.
 */
int buscar_cliente_por_id_cola(int id_cola) {
    for (int i = 0; i < num_clientes_activos; i++) {
        if (clientes[i].id_cola == id_cola) return i;
    }
    return -1;
}

/**
 * @brief Envía una notificación a todos los miembros de una sala.
 */
void difundir_notificacion(int indice_sala, const char* texto, int id_cola_excluida) {
    mensaje_t msg_notif;
    msg_notif.mtype = TIPO_NOTIFICACION;
    strncpy(msg_notif.texto, texto, MAX_TEXTO);

    sala_t* sala = &salas[indice_sala];
    for (int i = 0; i < sala->num_clientes; i++) {
        int indice_dest = sala->indices_clientes[i];
        if (clientes[indice_dest].id_cola != id_cola_excluida) {
            if (msgsnd(clientes[indice_dest].id_cola, &msg_notif, TAMANO_MENSAJE, 0) == -1) {
                if (errno != EIDRM) { 
                    perror("difundir_notificacion msgsnd");
                }
            }
        }
    }
}


/**
 * @brief Envía una respuesta directa a un cliente.
 */
void enviar_respuesta_a_cliente(int id_cola_cliente, tipo_mensaje_t tipo, const char* texto) {
    mensaje_t msg_resp;
    msg_resp.mtype = tipo;
    strncpy(msg_resp.texto, texto, MAX_TEXTO);
    // Se añade el chequeo de EIDRM para no mostrar un error si el cliente ya se desconectó.
    if (msgsnd(id_cola_cliente, &msg_resp, TAMANO_MENSAJE, 0) == -1 && errno != EIDRM) {
        perror("enviar_respuesta msgsnd");
    }
}


/**
 * @brief Guarda un mensaje en el archivo de log de la sala (Bonus).
 */
void registrar_mensaje_en_log(const char* nombre_sala, const char* nombre_usuario, const char* texto) {
    char ruta_archivo[200];
    snprintf(ruta_archivo, sizeof(ruta_archivo), "%s%s.log", RUTA_PERSISTENCIA, nombre_sala);
    
    FILE* archivo_log = fopen(ruta_archivo, "a");
    if (archivo_log == NULL) {
        perror("fopen log");
        return;
    }

    time_t ahora = time(NULL);
    char marca_tiempo[20];
    strftime(marca_tiempo, sizeof(marca_tiempo), "%Y-%m-%d %H:%M:%S", localtime(&ahora));

    fprintf(archivo_log, "[%s] %s: %s\n", marca_tiempo, nombre_usuario, texto);
    fclose(archivo_log);
}


/**
 * @brief Limpia la cola de mensajes del servidor antes de salir.
 */
void finalizar_servidor(int signum) {
    
    (void)signum; 
    printf("\n Cerrando el servidor...\n");
    if (id_cola_servidor != -1) {
        if (msgctl(id_cola_servidor, IPC_RMID, NULL) == -1) {
            perror("msgctl cleanup");
        } else {
            printf(" Cola del servidor eliminada correctamente.\n");
        }
    }
    exit(EXIT_SUCCESS);
}