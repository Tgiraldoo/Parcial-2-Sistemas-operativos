# Compilador y Opciones 
CC = gcc
# CFLAGS: Opciones del compilador
# -Wall -Wextra: Activa casi todas las advertencias. ¡Esencial para código de calidad!
# -g: Incluye información de depuración (para usar con gdb).
# -std=c99: Especifica el estándar de C a utilizar.
CFLAGS = -Wall -Wextra -g -std=c99

# LDFLAGS: Opciones del enlazador (linker)
# -lpthread: Enlaza con la librería de pthreads (necesaria para cliente.c)
LDFLAGS = -lpthread

# Objetivos (Targets) 
# El primer objetivo es el que se ejecuta por defecto con "make"
all: servidor cliente

# Regla para compilar el servidor
servidor: servidor.c common.h
	$(CC) $(CFLAGS) servidor.c -o servidor

# Regla para compilar el cliente
cliente: cliente.c common.h
	$(CC) $(CFLAGS) cliente.c -o cliente $(LDFLAGS)

# Regla para limpiar los archivos compilados
clean:
	rm -f servidor cliente


.PHONY: all clean prepare