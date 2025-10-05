#include "common.h"
#include <pthread.h>

// Variables Globales del Cliente 
static int id_cola_servidor = -1;
static int id_cola_privada = -1;
static char mi_nombre[MAX_NOMBRE];
static char sala_actual[MAX_NOMBRE] = "";
static volatile int seguir_corriendo = 1;

// Prototipos 
void finalizar_cliente(int signum);
void* hilo_receptor_mensajes(void* arg);
void procesar_entrada_usuario();
void enviar_comando_al_servidor(tipo_mensaje_t tipo, const char* sala, const char* texto);


/**
 * @brief Función principal del cliente.
 */
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <nombre_usuario>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    strncpy(mi_nombre, argv[1], MAX_NOMBRE - 1);

    signal(SIGINT, finalizar_cliente);

    key_t clave_servidor = ftok(RUTA_CLAVE_SERVIDOR, ID_PROYECTO);
    if (clave_servidor == -1) {
        perror("ftok servidor"); exit(EXIT_FAILURE);
    }
    id_cola_servidor = msgget(clave_servidor, 0);
    if (id_cola_servidor == -1) {
        perror("msgget servidor - ¿Está el servidor corriendo?");
        exit(EXIT_FAILURE);
    }

    id_cola_privada = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (id_cola_privada == -1) {
        perror("msgget cliente privado");
        exit(EXIT_FAILURE);
    }

    printf(" ¡Bienvenido al chat, %s! (ID Cola: %d)\n", mi_nombre, id_cola_privada);
    printf("Comandos: /join <sala>, /leave, /list, /users, /exit\n");

    pthread_t id_hilo_receptor;
    if (pthread_create(&id_hilo_receptor, NULL, hilo_receptor_mensajes, NULL) != 0) {
        perror("pthread_create");
        finalizar_cliente(0);
        exit(EXIT_FAILURE);
    }

    procesar_entrada_usuario();
    
    // Secuencia de Cierre Controlado
    // Cuando el bucle de entrada termina (por /exit o Ctrl+D)
    finalizar_cliente(0); 
    pthread_join(id_hilo_receptor, NULL);
    
    printf("Cliente desconectado.\n");
    return 0;
}


/**
 * @brief Hilo que lee la entrada del usuario y la procesa.
 */
void procesar_entrada_usuario() {
    char buffer[MAX_TEXTO + 20];
    while (seguir_corriendo) {
        printf("> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break; // Ctrl+D
        }
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0) continue;

        if (strncmp(buffer, "/join ", 6) == 0) {
            char nombre_sala[MAX_NOMBRE];
            sscanf(buffer + 6, "%s", nombre_sala);
            strncpy(sala_actual, nombre_sala, MAX_NOMBRE);
            enviar_comando_al_servidor(TIPO_UNION_SALA, sala_actual, "");
        } else if (strcmp(buffer, "/leave") == 0) {
            if (strlen(sala_actual) > 0) {
                enviar_comando_al_servidor(TIPO_ABANDONAR_SALA, sala_actual, "");
                strcpy(sala_actual, "");
            } else {
                printf("No estás en ninguna sala.\n");
            }
        } else if (strcmp(buffer, "/list") == 0) {
            enviar_comando_al_servidor(TIPO_LISTAR_SALAS, "", "");
        } else if (strcmp(buffer, "/users") == 0) {
            if (strlen(sala_actual) > 0) {
                enviar_comando_al_servidor(TIPO_LISTAR_USUARIOS, sala_actual, "");
            } else {
                printf("No estás en ninguna sala.\n");
            }
        } else if (strcmp(buffer, "/exit") == 0) {
            break; // Romper el bucle para iniciar el cierre
        } else if (buffer[0] == '/') {
            printf("Comando desconocido.\n");
        } else {
            if (strlen(sala_actual) > 0) {
                enviar_comando_al_servidor(TIPO_MENSAJE, sala_actual, buffer);
            } else {
                printf("No estás en una sala. Usa /join <sala>.\n");
            }
        }
    }
}


/**
 * @brief Hilo que escucha mensajes del servidor en la cola privada.
 */
void* hilo_receptor_mensajes(void* arg) {
    (void)arg; // Parámetro no usado
    mensaje_t msg;
    while (seguir_corriendo) {
        if (msgrcv(id_cola_privada, &msg, TAMANO_MENSAJE, 0, 0) == -1) {
            if (seguir_corriendo) { // Solo mostrar error si no estamos saliendo
                 perror("msgrcv cliente");
            }
            break;
        }
        printf("\r\033[K%s\n> ", msg.texto);
        fflush(stdout);
    }
    return NULL;
}


/**
 * @brief Construye y envía un mensaje al servidor.
 */
void enviar_comando_al_servidor(tipo_mensaje_t tipo, const char* sala, const char* texto) {
    if (id_cola_servidor == -1) return; // No enviar si ya nos estamos cerrando

    mensaje_t msg;
    msg.mtype = tipo;
    msg.id_cola_cliente = id_cola_privada;
    strncpy(msg.nombre_usuario, mi_nombre, MAX_NOMBRE);
    strncpy(msg.nombre_sala, sala, MAX_NOMBRE);
    strncpy(msg.texto, texto, MAX_TEXTO);

    if (msgsnd(id_cola_servidor, &msg, TAMANO_MENSAJE, 0) == -1) {
        // Ignorar el error "Identifier removed" que puede ocurrir si el servidor se cierra primero
        if (errno != EIDRM) {
            perror("msgsnd al servidor");
        }
    }
}


/**
 * @brief Limpia los recursos del cliente antes de salir.
 */
void finalizar_cliente(int signum) {
    static int ya_finalizado = 0;
    if (ya_finalizado) return;
    ya_finalizado = 1;
    
    seguir_corriendo = 0;
    
    if (signum != 0) { // Si es llamado por una señal
        printf("\n Desconectando y limpiando recursos...\n");
    }

    if (id_cola_servidor != -1) {
        enviar_comando_al_servidor(TIPO_CIERRE_CLIENTE, "", "");
    }
    
    if (id_cola_privada != -1) {
        // Esta llamada destruye la cola y desbloquea msgrcv en el hilo receptor
        msgctl(id_cola_privada, IPC_RMID, NULL);
    }

    if (signum != 0) {
        exit(0);
    }
}