#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

int main() {
    int fd;
    struct termios options;
    char *portname = "/dev/ttyACM0"; // Cambia esto según el puerto de tu Arduino
    char command[10];
    char input[5];

    // Abre el puerto serie
    fd = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("open_port: Unable to open /dev/ttyACM0 - ");
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
        // Leer la entrada del usuario
        fgets(input, sizeof(input), stdin);
        
        // Eliminar el carácter de nueva línea al final de la cadena
        input[strcspn(input, "\n")] = 0;

        // Comparar la entrada con "exit"
        if (strcmp(input, "exit") == 0) {
            break;
        }
        else {
            // Enviar comando al Arduino
            snprintf(command, sizeof(command), "%d", atoi(input)); // Enviar posición 90 grados
            strcat(command, "\n"); // Agregar un carácter de nueva línea al final
            write(fd, command, strlen(command)); // Enviar solo la longitud del comando
        }

        // Imprimir la entrada del usuario
        printf("Has escrito: %s\n", input);
    }

    close(fd);
    return 0;
}
