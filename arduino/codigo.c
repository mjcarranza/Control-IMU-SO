#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

#define MAX_SAMPLES 1000

// Función para configurar la conexión serial
int configure_serial(int fd) {
    struct termios tty;
    memset(&tty, 0, sizeof tty);

    if (tcgetattr(fd, &tty) != 0) {
        printf("Error %d from tcgetattr: %s\n", errno, strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag |= (CLOCAL | CREAD);    // habilita el receptor y establece localmente
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         // 8 bits de datos
    tty.c_cflag &= ~PARENB;     // sin paridad
    tty.c_cflag &= ~CSTOPB;     // 1 bit de stop
    tty.c_cflag &= ~CRTSCTS;    // sin control de flujo

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;       // sin eco
    tty.c_lflag &= ~ECHOE;      // sin eco de borrado
    tty.c_lflag &= ~ISIG;       // sin interpretación de caracteres INTR, QUIT, SUSP

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // sin control de flujo de software
    tty.c_iflag &= ~(ICRNL | INLCR);        // sin mapeo de CR a NL y viceversa

    tty.c_oflag &= ~OPOST; // sin procesamiento de salida

    // Establece el tiempo de espera para la lectura
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error %d from tcsetattr: %s\n", errno, strerror(errno));
        return -1;
    }

    return 0;
}

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

// Función para calcular la media de los valores
void calculate_average(int *values, int sample_count, const char *label) {
    long sum = 0;
    for (int i = 0; i < sample_count; i++) {
        sum += values[i];
    }
    double average = sum / (double)sample_count;
    printf("Media de %s: %f\n", label, average);
}

int main() {
    int fd;
    struct termios options;
    char *portname = "/dev/ArduinoDriver3"; // Set Arduino port
    char command[20];
    char inputIzq[5];
    char inputDer[5];

    // Abre el puerto serie
    fd = open(portname, O_RDWR);
    if (fd == -1) {
        perror("open_port: Unable to open /dev/ArduinoDriver3 - ");
        return -1;
    }

    // Configura el puerto serie
    tcgetattr(fd, &options);
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    options.c_cflag |= (CLOCAL | CREAD);
    tcsetattr(fd, TCSANOW, &options);

    // Asegúrate de que la conexión serie esté en modo bloqueante
    fcntl(fd, F_SETFL, 0);

    while (1) {
        // Leer la entrada del usuario para el servo Izq
        printf("Introduce la posición para el servo Izq:\n");
        fgets(inputIzq, sizeof(inputIzq), stdin);
        inputIzq[strcspn(inputIzq, "\n")] = 0; // Eliminar el carácter de nueva línea

        // Comparar la entrada con "exit"
        if (strcmp(inputIzq, "exit") == 0) {
            break;
        }

        // Leer la entrada del usuario para el servo Der
        printf("Introduce la posición para el servo Der:\n");
        fgets(inputDer, sizeof(inputDer), stdin);
        inputDer[strcspn(inputDer, "\n")] = 0; // Eliminar el carácter de nueva línea

        // Comparar la entrada con "exit"
        if (strcmp(inputDer, "exit") == 0) {
            break;
        }

        // Enviar comando al Arduino para ambos servos
        snprintf(command, sizeof(command), "%s,%s\n", inputIzq, inputDer);
        write(fd, command, strlen(command));
    }
    
    int gx_values[MAX_SAMPLES];
    int gy_values[MAX_SAMPLES];
    int gz_values[MAX_SAMPLES];
    int sample_count = 0;

    char read_buffer[256];
    memset(read_buffer, 0, sizeof(read_buffer));

    while (sample_count < MAX_SAMPLES) {
        int n_read = receive_data(fd, read_buffer, sizeof(read_buffer) - 1);
        if (n_read > 0) {
            read_buffer[n_read] = '\0'; // Asegura que la cadena esté terminada en NULL
            printf("Read: %s\n", read_buffer);

            // Extraer valores gx, gy y gz de la cadena
            int gx, gy;
            if (sscanf(read_buffer, "GX:%d GY:%d ", &gx, &gy) == 2) {
                gx_values[sample_count] = gx;
                gy_values[sample_count] = gy;

                sample_count++;
            }
        } else if (n_read < 0) {
            printf("Error %d reading from %s: %s\n", errno, portname, strerror(errno));
            break;
        }
        calculate_average(gx_values, sample_count, "GX");
        calculate_average(gy_values, sample_count, "GY");

        usleep(100000); // Espera un poco antes de intentar leer nuevamente
    }

    close(fd);
    return 0;
}
