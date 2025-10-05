# Reto 1: Sistema de Chat con Colas de Mensajes (System V)

Una implementación de un sistema de chat multi-sala en C, diseñado para sistemas operativos Linux.

##  Descripción del Proyecto

Este proyecto implementa un sistema de chat multi-usuario y multi-sala, utilizando **colas de mensajes de System V** como mecanismo de Comunicación Entre Procesos (IPC). La arquitectura cliente-servidor permite que múltiples usuarios se conecten, se unan a diferentes salas y conversen en tiempo real.


---

##  Características Principales

*   **Arquitectura Cliente-Servidor Robusta:** Un servidor central gestiona toda la lógica, mientras que los clientes son ligeros y se enfocan en la interfaz de usuario.

*   **Salas de Chat Dinámicas:** Las salas se crean automáticamente cuando el primer usuario se une, permitiendo una organización flexible de las conversaciones.

*   **Comunicación Asíncrona:** Gracias a los hilos, los clientes pueden recibir mensajes en tiempo real sin interrumpir lo que están escribiendo, ofreciendo una experiencia de usuario fluida.

*   **Gestión Segura de Recursos:** El sistema utiliza manejadores de señales (`signal`) para garantizar que todas las colas de mensajes se eliminen correctamente al cerrar el servidor (`Ctrl+C`) o los clientes, evitando dejar recursos huérfanos en el sistema.

*   **Eficiencia y Seguridad:** El código está optimizado para un bajo consumo de CPU y memoria. Gestión de usuarios y manejo seguro de strings con funciones como `strncpy` y `snprintf` para prevenir desbordamientos de búfer.

*   **Comandos Avanzados:** Interfaz enriquecida con comandos para `/list` (listar salas), `/users` (ver usuarios en la sala) y `/leave` (abandonar sala).

*   **Persistencia de Mensajes:** Todo el historial de chat se guarda de forma segura en archivos de texto (`historial/<sala>.log`), con marcas de tiempo para cada mensaje y evento del sistema.

---

##  Guía de Instalación y Uso

### 1. Prerrequisitos

Se necesita un entorno Linux (o WSL en Windows) con las herramientas de compilación básicas.

```bash
# Instalar herramientas de desarrollo (Ubuntu/Debian)
sudo apt update
sudo apt install -y build-essential

# Verificar que GCC esté instalado
gcc --version
```

### 2. Compilación

El `Makefile` incluido automatiza todo el proceso. Para compilar los ejecutables `servidor` y `cliente`, simplemente ejecute:

```bash
make
```


### 3. Ejecución

El servidor debe iniciarse antes que cualquier cliente.

*   **Paso 1: Iniciar el Servidor**
    Abra una terminal y ejecute:
    ```bash
    ./servidor
    ```
    El servidor se iniciará 

*   **Paso 2: Iniciar los Clientes**
    Abra **nuevas terminales** para cada cliente, proporcionando un nombre de usuario único como argumento.
    ```bash
    # En una segunda terminal
    ./cliente Maria

    # En una tercera terminal
    ./cliente Juan
    ```

---

## 💻 Comandos Disponibles

| Comando     | Argumento       | Descripción                                                          |
| :---------- | :-------------- | :------------------------------------------------------------------- |
| `/join`     | `<nombre_sala>` | Se une a una sala. Si no existe, la crea.                            |
| `/leave`    | (ninguno)       | Abandona la sala de chat actual.                                     |
| `/list`     | (ninguno)       | Muestra una lista de todas las salas activas.                        |
| `/users`    | (ninguno)       | Muestra los usuarios en la sala actual.                              |
| `/exit`     | (ninguno)       | Desconecta al cliente de forma segura y limpia los recursos.         |

Cualquier texto que no comience con `/` será enviado como un mensaje a la sala actual.

*   **Video:**(https://drive.google.com/file/d/1YSiM710B9-0luvaamH2uLQwixuwx8BDP/view?usp=sharing)