#define _WIN32_WINNT 0x0600
#include "../common/protocol.h"
#include "../common/crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

static SOCKET sockfd;
static uint8_t enc_key[KEY_LENGTH];

unsigned __stdcall receive_messages(void *arg) {
    uint8_t enc_buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(sockfd, (char*) enc_buffer, sizeof(enc_buffer) - 1, 0)) > 0) {
        xor_crypt(enc_buffer, bytes_received, enc_key);
        enc_buffer[bytes_received] = '\0';
        printf("%s", (char*) enc_buffer);
        fflush(stdout);
    }

    printf("\ndesconectado del servidor.\n");
    closesocket(sockfd);
    WSACleanup();
    exit(0);

    return 0;
}

int main(int argc, char *argv[]) {
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;

    WSADATA wsaData;
    struct sockaddr_in server_addr;

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        printf("WSAStartup fallido\n");
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        printf("socket fallido\n");
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("connect fallido\n");
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    // intercambio de claves
    uint64_t client_priv = generate_private_key();
    uint64_t client_pub = compute_public_key(client_priv);
    uint64_t server_pub;

    if (recv(sockfd, (char*) &server_pub, sizeof(server_pub), 0) != sizeof(server_pub)) {
        printf("error recibiendo clave pública\n");
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    if (send(sockfd, (char*)&client_pub, sizeof(client_pub), 0) != sizeof(client_pub)) {
        printf("error enviando clave pública\n");
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    uint64_t shared_secret = compute_shared_secret(server_pub, client_priv);
    kdf_derive(shared_secret, enc_key, KEY_LENGTH);

    printf("conectado a %s:%d\n", server_ip, port);
    printf("comandos: /nick <nombre>, /join <sala>\n");

    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, receive_messages, NULL, 0, NULL);
    if (hThread == NULL) {
        printf("error creando hilo\n");
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }
    CloseHandle(hThread);

    char message[BUFFER_SIZE];
    while (1) {
        if (fgets(message, sizeof(message), stdin) == NULL) break;

        int first_char = strspn(message, " \t\n\r");
        if (message[first_char] == '\0') continue;

        // los códigos ANSI puede que no funcionen en cmd.exe
        // pero funcionan en la terminal moderna
        printf("\033[1A\r\033[K");
        fflush(stdout);

        if (message[0] != '/') {
            char display[BUFFER_SIZE];
            strcpy(display, message);
            display[strcspn(display, "\n")] = '\0';
            printf("[TÚ] %s\n", display);
        }

        size_t msg_len = strlen(message) + 1;
        uint8_t *enc_msg = malloc(msg_len);
        memcpy(enc_msg, message, msg_len);
        xor_crypt(enc_msg, msg_len, enc_key);

        if (send(sockfd, (char*) enc_msg, (int) msg_len, 0) == SOCKET_ERROR) {
            printf("send fallido\n");
            free(enc_msg);
            break;
        }
        free(enc_msg);
    }

    closesocket(sockfd);
    WSACleanup();

    return 0;
}
