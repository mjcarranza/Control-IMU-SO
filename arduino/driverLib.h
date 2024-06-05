// driverLib.h
#include <stdlib.h>
#include <errno.h>

#ifndef CODIGO_ARD_H
#define CODIGO_ARD_H
#define MAX_SAMPLES 500
#define MOVING_AVERAGE_WINDOW 10

// Función para configurar la conexión serial
int configure_serial(int fd);

// Función para recibir datos del Arduino
int receive_data(int fd, char *buffer, size_t length);

// Función para enviar datos al Arduino
int send_data(int fd, const char *data);

#endif // CODIGO_ARD_H

