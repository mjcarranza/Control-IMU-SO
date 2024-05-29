#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

int main() {
    int fd;
    struct termios options;
    char *portname = "/dev/ttyACM0"; // Set Arduino port
    char command[20];
    char inputIzq[5];
    char inputDer[5];

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

    close(fd);
    return 0;
}
