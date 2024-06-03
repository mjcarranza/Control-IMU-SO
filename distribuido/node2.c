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
    unsigned char ciphertext[128]; // Buffer para recibir el texto cifrado
    unsigned char ciphertextSend[128];
    unsigned char decryptedtext[128]; // Buffer para almacenar el texto descifrado
    char posiciones[20];

    MPI_Init(&argc, &argv); // Inicializar el entorno MPI
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // Obtener el rango del proceso actual
    MPI_Comm_size(MPI_COMM_WORLD, &size); // Obtener el tamaño del comunicador (número de procesos)
    // Clave y IV para cifrado (esto es solo un ejemplo, en producción debes gestionarlos de manera segura)
    unsigned char *key = (unsigned char *)"01234567890123456789012345678901"; // Clave de 256 bits
    unsigned char *iv = (unsigned char *)"0123456789012345"; // IV de 128 bits


    // aqui se debe enviar los datos previamente procesados a el nodo 2
    if(rank != 0){
        while (1)
        {
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

    MPI_Finalize(); // Finalizar el entorno MPI
    return 0; // Salir del programa
}




// main se encarga de 
// recibir los datos del arduino (ver como se envian)
// encriptar los datos 
// enviar datos al nodo 2

// recibir los datos procesados del nodo 2
// decifrar los datos recibidos 
// enviar las coordenadas al arduino para mover los servos

// se recibe una cadena de texto como esta --->   22:42:39.695 -> GX:-1351 GY:1068