#include "encriptLib.h"

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