#include "../common/protocol.h"
#include "../common/crypto.h"
#include "../common/kdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int sockfd;
static uint8_t enc_key[KEY_LENGTH];

void *receive_messages(void *arg) {
    uint8_t enc_buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(sockfd, enc_buffer, sizeof(enc_buffer) - 1, 0)) > 0) {
        xor_crypt(enc_buffer, bytes_received, enc_key);
        enc_buffer[bytes_received] = '\0';
        printf("%s", (char*) enc_buffer);
        fflush(stdout);
    }

    printf("\ndesconectado del servidor.\n");
    close(sockfd);
    exit(0);

    return NULL;
}

int main(int argc, char *argv[]) {
    // por defecto: localhost:6677
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;

    struct sockaddr_in server_addr;

    // crear socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        exit(1);
    }

    /* INTERCAMBIO DE CLAVES DIFFIE-HELLMAN */
    uint64_t client_priv = generate_private_key();
    uint64_t client_pub = compute_public_key(client_priv);
    uint64_t server_pub;

    // recibir clave pública del server
    if (recv(sockfd, &server_pub, sizeof(server_pub), 0) != sizeof(server_pub)) {
        fprintf(stderr, "error al recibir clave pública del server\n");
        close(sockfd);
        exit(1);
    }

    // enviar clave pública del cliente
    if (send(sockfd, &client_pub, sizeof(client_pub), 0) != sizeof(client_pub)) {
        fprintf(stderr, "error enviando clave pública\n");
        close(sockfd);
        exit(1);
    }

    //calcular secreto compartido y derivar clave
    uint64_t shared_secret = compute_shared_secret(server_pub, client_priv);
    kdf_derive(shared_secret, enc_key, KEY_LENGTH);

    printf("conectado a %s:%d\n", server_ip, port);
    printf("comandos: /nick <nombre>; /join <sala>\n");

    // crear hilo receptor
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("pthread_create");
        close(sockfd);
        exit(1);
    }

    // detectar input
    char message[BUFFER_SIZE];
    while (1) {
        if (fgets(message, sizeof(message), stdin) == NULL) break;

        // ignorar mensajes vacíos
        int first_char = strspn(message, " \t\n\r");
        if (message[first_char] == '\0') continue;

        // limpiar línea anterior (para el eco local)
        printf("\033[1A\r\033[K");
        fflush(stdout);

        // mostrar eco local (si no es comando)
        if (message[0] != '/') {
            char display[BUFFER_SIZE];
            strcpy(display, message);
            display[strcspn(display, "\n")] = '\0';
            printf("[TÚ] %s\n", display);
        }

        // encriptar y enviar
        size_t msg_len = strlen(message) + 1;
        uint8_t *enc_msg = malloc(msg_len);
        memcpy(enc_msg, message, msg_len);
        xor_crypt(enc_msg, msg_len, enc_key);

        if (send(sockfd, enc_msg, msg_len, 0) < 0) {
            perror("send");
            free(enc_msg);
            break;
        }
        free(enc_msg);
    }

    close(sockfd);
    return 0;
}
