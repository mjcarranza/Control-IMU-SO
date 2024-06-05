/* Ciente que se conecta por medio de sockets con protocolo TCP */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "driverLib.h" // se incluye la libreria hacia el driver
#include <asm-generic/fcntl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <termios.h>



#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_SAMPLES 500
#define MOVING_AVERAGE_WINDOW 10

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

void initialize_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX* create_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_client_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char read_buffer[256];
    memset(read_buffer, 0, sizeof(read_buffer));
    const int arduino_interval_ms = 100;  // Intervalo de envío del Arduino en ms
    const int usleep_interval = (arduino_interval_ms - 10) * 1000;  // Usleep en microsegundos, ajustado
    int gx_values[MAX_SAMPLES];
    int gy_values[MAX_SAMPLES];
    int gz_values[MAX_SAMPLES];
    int sample_count = 0;

    initialize_openssl();
    SSL_CTX *ctx = create_context();

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    // variables para la comunicacion serial
    int fd;
    struct termios options;
    char *portname = "/dev/ArduinoDriver3"; // Usar este puerto para el driver
    //const char *portname = "/dev/ttyACM1";

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

    // Asegurar que la conexión serie esté en modo bloqueante
    fcntl(fd, F_SETFL, 0);

    // Crear el socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convertir direcciones IPv4 e IPv6 de texto a binario
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Conectar al servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    while (1) {
        // leer los datos del arduino
        while (sample_count < MAX_SAMPLES) {
            int n_read = receive_data(fd, read_buffer, sizeof(read_buffer) - 1);
            if (n_read > 0) {
                read_buffer[n_read] = '\0'; // Asegura que la cadena esté terminada en NULL
                printf("Read: %s\n", read_buffer);

                // Extraer valores gx, gy y gz de la cadena
                int gx, gy,dx,dy;
                if (sscanf(read_buffer, "GX:%d GY:%d ", &gx, &gy) == 2) {
                    gx_values[sample_count] = gx;
                    gy_values[sample_count] = gy;

                    sample_count++;
                }
            } else if (n_read < 0) {
                printf("Error %d reading from %s: %s\n", errno, portname, strerror(errno));
                break;
            }
            usleep(usleep_interval); // Espera ajustada antes de intentar leer nuevamente
        }

        

        if (SSL_connect(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        } else {
            // Empaquetar los datos en un único buffer
            memcpy(buffer, gx_values, sample_count * sizeof(int));
            memcpy(buffer + sample_count * sizeof(int), gy_values, sample_count * sizeof(int));

            // Enviar el buffer al servidor
            SSL_write(ssl, buffer, 2 * sample_count * sizeof(int));
            printf("Data sent\n");
        }

        // leer datos enviados desde el servidor
        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        } else {
            // Recibir datos del cliente
            int valread = SSL_read(ssl, buffer, BUFFER_SIZE);
            if (valread > 0) {
                printf(valread);
            } else {
                printf("No data received\n");
            }
        }
        printf("Server: %s\n", buffer);
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    cleanup_openssl();

    return 0;
}
