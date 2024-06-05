#include "driverLib.h"

// Función para recibir datos del Arduino
int receive_data(int fd, char *buffer, size_t length) {
    int total_read = 0;
    while (total_read < length) {
        int n_read = read(fd, buffer + total_read, length - total_read);
        if (n_read < 0) {
            printf("Error %d reading from serial: %s\n", errno, strerror(errno));
            return -1;
        } else if (n_read == 0) {
            break; // No hay más datos disponibles
        }
        total_read += n_read;
    }
    return total_read;
}

// Función para enviar datos al Arduino
int send_data(int fd, const char *data) {
    int length = strlen(data);
    int total_written = 0;
    while (total_written < length) {
        int n_written = write(fd, data + total_written, length - total_written);
        if (n_written < 0) {
            printf("Error %d writing to serial: %s\n", errno, strerror(errno));
            return -1;
        }
        total_written += n_written;
    }
    return total_written;
}