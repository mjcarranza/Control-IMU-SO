# Control-IMU-SO

### Proyecto #2 Principios de Sistemas Operativos Instituto Tecnológico de Costa Rica



Para extender el programa y hacerlo funcionar en dos computadoras diferentes usando MPI, necesitarás configurar una red de MPI entre las computadoras. Aquí hay una guía paso a paso:

### 1. Configuración de SSH sin contraseña

Asegúrate de que las dos computadoras puedan comunicarse entre sí mediante SSH sin contraseña:

1. En la **máquina 1**, genera una clave SSH (si aún no tienes una):

    ```sh
    ssh-keygen -t rsa
    ```

2. Copia la clave pública a la **máquina 2**:

    ```sh
    ssh-copy-id user@maquina2
    ```

   Reemplaza `user` con tu nombre de usuario en la **máquina 2** y `maquina2` con la dirección IP o el nombre de host de la **máquina 2**.

3. Verifica que puedes iniciar sesión en la **máquina 2** desde la **máquina 1** sin necesidad de contraseña:

    ```sh
    ssh user@maquina2
    ```

### 2. Instalación de OpenMPI en Ambas Máquinas

Asegúrate de que OpenMPI esté instalado en ambas máquinas:

```sh
sudo apt-get update
sudo apt-get install openmpi-bin openmpi-common libopenmpi-dev
```

### 3. Crear un Archivo de Hosts

Crea un archivo de hosts en la **máquina 1** que contenga las direcciones IP o los nombres de host de las máquinas que deseas usar. Por ejemplo, crea un archivo llamado `hosts`:

```sh
echo "maquina1 slots=1" > hosts
echo "maquina2 slots=1" >> hosts
```

### 4. Código Actualizado para Distribución en Dos Máquinas

El código del programa en C no necesita modificaciones adicionales para funcionar en múltiples máquinas. Aquí está el código actualizado para referencia:

```c
#include <mpi.h> // Biblioteca para MPI
#include <openssl/evp.h> // Biblioteca para cifrado OpenSSL
#include <string.h> // Biblioteca para manejo de cadenas
#include <stdlib.h> // Biblioteca estándar
#include <stdio.h> // Biblioteca para entrada/salida

// Función para cifrar datos
int encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key, unsigned char *iv, unsigned char *ciphertext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); // Crear un nuevo contexto de cifrado
    int len; // Variable para almacenar la longitud del texto cifrado
    int ciphertext_len; // Variable para almacenar la longitud total del texto cifrado

    // Inicializar el contexto de cifrado con el algoritmo AES-256-CBC, la clave y el IV
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

    // Realizar la operación de cifrado en el texto plano
    EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len);
    ciphertext_len = len; // Actualizar la longitud del texto cifrado

    // Finalizar el cifrado (manejo de cualquier bloque restante)
    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    ciphertext_len += len; // Actualizar la longitud total del texto cifrado

    EVP_CIPHER_CTX_free(ctx); // Liberar el contexto de cifrado
    return ciphertext_len;
}

// Función para descifrar datos
int decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key, unsigned char *iv, unsigned char *plaintext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); // Crear un nuevo contexto de descifrado
    int len; // Variable para almacenar la longitud del texto descifrado
    int plaintext_len; // Variable para almacenar la longitud total del texto descifrado

    // Inicializar el contexto de descifrado con el algoritmo AES-256-CBC, la clave y el IV
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

    // Realizar la operación de descifrado en el texto cifrado
    EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len);
    plaintext_len = len; // Actualizar la longitud del texto descifrado

    // Finalizar el descifrado (manejo de cualquier bloque restante)
    EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    plaintext_len += len; // Actualizar la longitud total del texto descifrado

    EVP_CIPHER_CTX_free(ctx); // Liberar el contexto de descifrado
    plaintext[plaintext_len] = '\0'; // Agregar terminador null al final del texto descifrado
    return plaintext_len;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv); // Inicializar el entorno MPI
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // Obtener el rango del proceso actual
    MPI_Comm_size(MPI_COMM_WORLD, &size); // Obtener el tamaño del comunicador (número de procesos)

    // Clave y IV para cifrado (esto es solo un ejemplo, en producción debes gestionarlos de manera segura)
    unsigned char *key = (unsigned char *)"01234567890123456789012345678901"; // Clave de 256 bits
    unsigned char *iv = (unsigned char *)"0123456789012345"; // IV de 128 bits

    if (rank == 0) { // Código para el proceso principal (rank 0)
        char message[] = "Hello, World!"; // Mensaje a cifrar y enviar
        unsigned char ciphertext[128]; // Buffer para almacenar el texto cifrado
        int ciphertext_len;

        // Cifrar el mensaje
        ciphertext_len = encrypt((unsigned char*)message, strlen(message), key, iv, ciphertext);

        // Enviar el mensaje cifrado a todos los demás procesos
        for (int i = 1; i < size; i++) {
            MPI_Send(ciphertext, ciphertext_len, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD);
        }
    } else { // Código para los procesos receptores (rank 1 a size-1)
        unsigned char ciphertext[128]; // Buffer para recibir el texto cifrado
        unsigned char decryptedtext[128]; // Buffer para almacenar el texto descifrado
        int ciphertext_len;
        MPI_Status status;

        // Recibir el mensaje cifrado del proceso principal
        MPI_Recv(ciphertext, 128, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &ciphertext_len);

        // Descifrar el mensaje
        decrypt(ciphertext, ciphertext_len, key, iv, decryptedtext);

        // Mostrar el mensaje descifrado
        printf("Proceso %d recibió y descifró el mensaje: %s\n", rank, decryptedtext);
    }

    MPI_Finalize(); // Finalizar el entorno MPI
    return 0; // Salir del programa
}
```

### 5. Compilación y Distribución del Código

Compila el código en la **máquina 1** y copia el ejecutable a la **máquina 2**:

```sh
mpicc -o distribuidor distribuidor.c -lssl -lcrypto
scp distribuidor user@maquina2:~/path/to/your/directory
```

### 6. Ejecución del Programa

Finalmente, ejecuta el programa usando `mpirun` en la **máquina 1** especificando el archivo de hosts:

```sh
mpirun -np 2 --hostfile hosts ./distribuidor
```

Esto iniciará el programa en las dos máquinas especificadas en el archivo `hosts`.

### Nota Final

Asegúrate de que los firewalls en ambas máquinas permitan el tráfico necesario para MPI y que el nombre del host o la dirección IP en el archivo `hosts` sean correctos y accesibles. También, verifica que los directorios y permisos sean correctos para evitar problemas de acceso.