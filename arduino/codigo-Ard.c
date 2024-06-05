#include <stdio.h>
#include <mpi.h>
#include <openssl/evp.h> // Biblioteca para cifrado OpenSSL
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include "driverLib.h" // se incluye la libreria hacia el driver
#include "encriptLib.h" // se incluye libreria para realizar la encriptacion de los datos


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

// Función para calcular la media de los valores
double calculate_average(int *values, int count) {
    long sum = 0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / (double)count;
}

// Función para calcular la media móvil y comparar
int calculate_moving_average_and_compare(int *values, int sample_count, const char *label) {
    if (sample_count < MOVING_AVERAGE_WINDOW) {
        printf("No hay suficientes muestras para calcular la media móvil de %s\n", label);
        return -1;
    }

    double previous_average = calculate_average(values + sample_count - MOVING_AVERAGE_WINDOW - 1, MOVING_AVERAGE_WINDOW);
    double current_average = calculate_average(values + sample_count - MOVING_AVERAGE_WINDOW, MOVING_AVERAGE_WINDOW);

    printf("Media móvil anterior de %s: %f\n", label, previous_average);
    printf("Media móvil actual de %s: %f\n", label, current_average);

    if (abs(current_average - previous_average) > 200.0) { // Ajusta el umbral según sea necesario
        printf("Cambio significativo en la media móvil de %s\n", label);
        return 1;
    }else {return 0;}
}


int main() {
    unsigned char ciphertext[128]; // Buffer para recibir el texto cifrado
    unsigned char decryptedtext[128]; // Buffer para almacenar el texto descifrado
    int ciphertext_len;
    MPI_Status status;
    
    MPI_Init(&argc, &argv); // Inicializar el entorno MPI
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // Obtener el rango del proceso actual
    MPI_Comm_size(MPI_COMM_WORLD, &size); // Obtener el tamaño del comunicador (número de procesos)
    // Clave y IV para cifrado (esto es solo un ejemplo, en producción debes gestionarlos de manera segura)
    unsigned char *key = (unsigned char *)"01234567890123456789012345678901"; // Clave de 256 bits
    unsigned char *iv = (unsigned char *)"0123456789012345"; // IV de 128 bits

    int fd;
    struct termios options;
    //char *portname = "/dev/ArduinoDriver3"; // Set Arduino port
    const char *portname = "/dev/ttyACM1";
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

    // Asegurar que la conexión serie esté en modo bloqueante
    fcntl(fd, F_SETFL, 0);

    // Ejecutar todo el proceso dentro de este while
    while(1){
        if(rank == 0){  // si estamos en el nodo master
            // en esta parte se tiene que recibir los datos del arduino y procesarlos para enviarlos
            char message[] = "-1351,1068"; // Mensaje a cifrar y enviar
            unsigned char ciphertext[20]; // Buffer para almacenar el texto cifrado
            int ciphertext_len;

            // Cifrar el mensaje
            ciphertext_len = encrypt((unsigned char*)message, strlen(message), key, iv, ciphertext);

            // Enviar el mensaje cifrado a todos los demás procesos
            for (int i = 1; i < size; i++) {
                MPI_Send(ciphertext, ciphertext_len, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD);
                printf("Mensaje enviado al nodo 1");
            }

            // Recibir el mensaje cifrado del Nodo 1 con las coordenadas para los servos
            MPI_Recv(ciphertext, 128, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &ciphertext_len);

            // Descifrar el mensaje
            decrypt(ciphertext, ciphertext_len, key, iv, decryptedtext);

            // Mostrar el mensaje que llega desde el nodo 1
            printf("El nodo %d envio el mensaje: %s\n", rank, decryptedtext);

            // enviar las posiciones a los servos
        }else{
            MPI_Status status;
            MPI_Recv(ciphertext, 128, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &status);
            int ciphertext_len;
            MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &ciphertext_len);

            decrypt(ciphertext, ciphertext_len, key, iv, decryptedtext); // el texto decifrado se guarda en decryptedtext

            // en esta parte se hace el analisis estadistico de los datos 
            // y se calculan las posiciones de los servos

            // encriptar los datos a enviar (estan en la variable posiciones)
            ciphertext_len = encrypt((unsigned char*)posiciones, strlen(posiciones), key, iv, ciphertextSend);

            // enviar de vuelta los datos al nodo 1
            MPI_Send(&ciphertextSend, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            
        }
    }
    

    //entrada del servo
    /*while (1) {
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
    }*/
    
    int gx_values[MAX_SAMPLES];
    int gy_values[MAX_SAMPLES];
    int gz_values[MAX_SAMPLES];
    int sample_count = 0;

    char read_buffer[256];
    memset(read_buffer, 0, sizeof(read_buffer));

    const int arduino_interval_ms = 100;  // Intervalo de envío del Arduino en ms
    const int usleep_interval = (arduino_interval_ms - 10) * 1000;  // Usleep en microsegundos, ajustado

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

                // Calcular y comparar las medias móviles cuando hay suficientes muestras
                if (sample_count >= MOVING_AVERAGE_WINDOW + 1) {
                    dx=calculate_moving_average_and_compare(gx_values, sample_count, "GX");
                    dy=calculate_moving_average_and_compare(gy_values, sample_count, "GY");
                    printf("Media final de GX: %f\n", calculate_average(gx_values, sample_count));
                    printf("Media final de GY: %f\n", calculate_average(gy_values, sample_count));

                    printf("cambio dX: %d\n", dx);
                    printf("cambio dY: %d\n", dy);

                    char send_data_str[] = "Hello Arduino\n";


                    /*
                    char str1[12];  
                    char str2[12];
                    
                    // Convertimos los enteros a cadenas
                    snprintf(str1, sizeof(str1), "%d", dx);
                    snprintf(str2, sizeof(str2), "%d", dy);
                    
                    // Calculamos el tamaño total necesario para la cadena resultante
                    int totalLength = strlen("dx") +strlen(str1) + strlen("dy") + strlen(str2) + strlen(send_data_str) + 1; // +1 para el carácter nulo
                    
                    // Definimos el array resultante
                    char result[totalLength];
                    
                    // Inicializamos la cadena resultante con la primera cadena
                    strcpy(result, str1);
                    
                    // Concatenamos la segunda cadena
                    strcat(result, str2);
                    
                    // Concatenamos el texto adicional
                    strcat(result, send_data_str);*/

                    if (send_data(fd, send_data_str) < 0) {
                        close(fd);
                        return -1;
                    }
                    usleep(100000);
                }
            }
        } else if (n_read < 0) {
            printf("Error %d reading from %s: %s\n", errno, portname, strerror(errno));
            break;
        }
        usleep(usleep_interval); // Espera ajustada antes de intentar leer nuevamente
    }

    MPI_Finalize(); // Finalizar el entorno MPI
    close(fd);
    return 0;
}
