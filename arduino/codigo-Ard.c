#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_SAMPLES 500
#define MOVING_AVERAGE_WINDOW 10

#define PORT 8080
#define BUFFER_SIZE 2048

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

    method = SSLv23_server_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

void configure_context(SSL_CTX *ctx) {
    SSL_CTX_set_ecdh_auto(ctx, 1);

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
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


int main(int argc, char** argv) {

    // variables para la inicializacion de OpenMPI
    MPI_Status status;
    MPI_Init(&argc, &argv); // Inicializar el entorno MPI
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // Obtener el rango del proceso actual
    MPI_Comm_size(MPI_COMM_WORLD, &size); // Obtener el tamaño del comunicador (número de procesos)
    

    // variables para la lectura y descomposicion de los datos del IMU 
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    int sample_count = 500;
    int contM = 0;
    int contS = 0;

    initialize_openssl();
    SSL_CTX *ctx = create_context();
    configure_context(ctx);
    int gx_values[MAX_SAMPLES];
    int gy_values[MAX_SAMPLES];


    // Crear el socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Asignar dirección y puerto
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Enlazar el socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Escuchar las conexiones entrantes
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Aceptar una conexión
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, new_socket);


    // Ejecutar todo el proceso dentro de este while
    while(1){
        MPI_Status status;
        // recibir los datos desde el cliente
        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        } else {
            // Recibir datos del cliente
            int valread = SSL_read(ssl, buffer, BUFFER_SIZE);
            if (valread > 0) {
                // los valores se guardan en gx y gy
                memcpy(gx_values, buffer, sample_count * sizeof(int));
                memcpy(gy_values, buffer + sample_count * sizeof(int), sample_count * sizeof(int));

            } else {
                printf("No data received\n");
            }
        }

        
        // si estamos en el nodo master, se ejecuta el analisis estadistico para dx
        if(rank == 0){  
            // Prepara un buffer para enviar el arreglo dy
            int gy_size = 500; // Tamaño del arreglo gy
            int dx;
            int *gy_buffer = malloc(gy_size * sizeof(int)); 
            memcpy(gy_buffer, gy_values, gy_size * sizeof(int)); // Copia los datos de gy a gy_buffer
            MPI_Send(gy_buffer, gy_size, MPI_INT, 0, 0, MPI_COMM_WORLD);// Enviar el arreglo dy al proceso slave
            free(gy_buffer);// Libera la memoria del buffer

            // hacer el procesamiento estadistico para dx
            while (contM < MAX_SAMPLES) {
                if (contM >= MOVING_AVERAGE_WINDOW + 1) {
                    dx = calculate_moving_average_and_compare(gx_values, contM, "GX");
                    // recibir los datos del nodo slave
                    MPI_Status status;
                    int gy_size;
                    int dy;
                    MPI_Recv(&gy_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status); // Recibe el tamaño del arreglo dy enviado por el nodo maestro
                    int *gy = malloc(gy_size * sizeof(int)); // Aloja memoria para almacenar el arreglo dy
                    MPI_Recv(gy, gy_size, MPI_INT, 0, 0, MPI_COMM_WORLD, &status); // Recibe el arreglo dy
                    // pasar los datos a cadena ed caracteres
                    char cadx[20], cady[20];
                    sprintf(cadx, "%d", dx); // el resultado esta en cadena
                    sprintf(cady, "%d", dy); // el resultado esta en cadena

                    // empaquetar resulatdos 
                    char resultado[100]; // Tamaño suficientemente grande para contener ambas cadenas y la coma
                    strcpy(resultado, cadx);
                    strcat(resultado, ",");
                    strcat(resultado, cady);
                    // encriptar los datos
                    
                    // enviar los datos encriptados
                    if (SSL_connect(ssl) <= 0) {
                        ERR_print_errors_fp(stderr);
                    } else {
                        // Enviar el buffer al servidor
                        SSL_write(ssl, resultado, 2 * sample_count * sizeof(int));
                        printf("Data sent\n");
                    }
                    printf("Data sent to client\n");
                }
                contM ++;
            }
            
            


            
        }else{ // si estamos en el nodo slave, recibir, ejecutar y enviar 
            MPI_Status status;
            int gy_size;
            int dy;
            MPI_Recv(&gy_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status); // Recibe el tamaño del arreglo dy enviado por el nodo maestro
            int *gy_buffer = malloc(gy_size * sizeof(int)); // Aloja memoria para almacenar el arreglo dy
            MPI_Recv(gy_buffer, gy_size, MPI_INT, 0, 0, MPI_COMM_WORLD, &status); // Recibe el arreglo dy
            // Procesa los datos recibidos
            // hacer el procesamiento estadistico para dx
            while (contS < MAX_SAMPLES) {
                if (contS >= MOVING_AVERAGE_WINDOW + 1) {
                    dy = calculate_moving_average_and_compare(gy_values, contS, "GY");
                    // Prepara un buffer para enviar el  dy
                    int dy_size = 10; // Tamaño del  gy
                    int *dy_buffer = malloc(dy_size * sizeof(int)); 
                    memcpy(dy_buffer, dy, dy_size * sizeof(int)); // Copia los datos de gy a gy_buffer
                    MPI_Send(dy_buffer, dy_size, MPI_INT, 0, 0, MPI_COMM_WORLD);// Enviar el arreglo dy al proceso slave
                    free(dy_buffer);// Libera la memoria del buffer
                }
                contS ++;
            }
            
            free(gy_buffer);
            
        }
    }

    // finalizacion del programa
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(new_socket);
    close(server_fd);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    MPI_Finalize(); // Finalizar el entorno MPI
    return 0;
}

                    // pasar los datos a cadena ed caracteres
                    // empaquetar resulatdos 
                    // encriptar los datos
                    // enviar los datos encriptados





/* TAREAS PARA HOY

- Meter lo del arduino en la parte de ejecucion distribuida

- Hacer que el arduino reciba la cadena de posiciones para los servos correctamente

- Hacer que este programa termine de manera "elegante"

- hacer lo del envio de datos entre nodos por medio de cifrado

- Verificar que los datos que se leen luego de desencriptarlos sean los correctos en ambos nodos

*/

/*
// Calcular y comparar las medias móviles cuando hay suficientes muestras
                if (sample_count >= MOVING_AVERAGE_WINDOW + 1) {
                    dx=calculate_moving_average_and_compare(gx_values, sample_count, "GX");
                    dy=calculate_moving_average_and_compare(gy_values, sample_count, "GY");
                    printf("Media final de GX: %f\n", calculate_average(gx_values, sample_count));
                    printf("Media final de GY: %f\n", calculate_average(gy_values, sample_count));

                    printf("cambio dX: %d\n", dx);
                    printf("cambio dY: %d\n", dy);

                    char send_data_str[] = "Hello Arduino\n";


                    if (send_data(fd, send_data_str) < 0) {
                        close(fd);
                        return -1;
                    }
                    usleep(100000);
                }

*/




/*
    // Enviar comando al Arduino para ambos servos
    snprintf(command, sizeof(command), "%s,%s\n", inputIzq, inputDer);
    write(fd, command, strlen(command));
*/



/* Recibir el mensaje cifrado del Nodo 1 con las coordenadas para los servos
MPI_Recv(ciphertext, 128, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &status);
MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &ciphertext_len);

// Descifrar el mensaje
decrypt(ciphertext, ciphertext_len, key, iv, decryptedtext);

// Mostrar el mensaje que llega desde el nodo 1
printf("El nodo %d envio el mensaje: %s\n", rank, decryptedtext);
*/



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