#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <process.h>            // para _beginthreadex
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#define SOCKET_ERROR_RETURN INVALID_SOCKET
#else
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int socket_t;
#define CLOSE_SOCKET close
#define SOCKET_ERROR_RETURN -1
#endif

#define BUFFER_SIZE 1024
#define TRUE 1

static socket_t sockfd;

/* HILO RECEPTOR */
#ifdef _WIN32
static unsigned __stdcall receive_messages(void *arg)
#else
static void *receive_messages(void *arg)
#endif
{
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
        fflush(stdout);
    }

    if (bytes_received == 0) {
        printf("\ndesconectado.\n");
    } else {
#ifdef _WIN32
        fprintf(stderr, "recv error: %d\n", WSAGetLastError());
#else
        perror("recv");
#endif
    }

    CLOSE_SOCKET(sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
    exit(0);
}

/* MAIN ENTRYPOINT */
int main(int argc, char *argv[]) {
    // fallback a localhost
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    const int server_port = (argc > 2) ? atoi(argv[2]) : 6677;
    struct sockaddr_in server_addr;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        exit(1);
    }
#endif

    // crear socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == SOCKET_ERROR_RETURN) {
#ifdef _WIN32
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
#else
        perror("socket");
#endif
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
#ifdef _WIN32
        fprintf(stderr, "inet_pton failed\n");
#else
        perror("inet_pton");
#endif
        CLOSE_SOCKET(sockfd);
        exit(1);
    }

    // conectar a server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
#ifdef _WIN32
        fprintf(stderr, "connect failed: %d\n", WSAGetLastError());
#else
        perror("connect");
#endif
        CLOSE_SOCKET(sockfd);
        exit(1);
    }

    printf("conectado a %s:%d\n", server_ip, server_port);
    printf("comandos: /nick <nombre>, /join <sala>\n");

    // crear hilo receptor
#ifdef _WIN32
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, receive_messages, NULL, 0, NULL);
    if (hThread == NULL) {
        fprintf(stderr, "_beginthreadex failed\n");
        CLOSE_SOCKET(sockfd);
        WSACleanup();
        exit(1);
    }
    CloseHandle(hThread);   // desadjuntar
#else
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("pthread_create");
        CLOSE_SOCKET(sockfd);
        exit(1);
    }
#endif

    // leer input y enviar
    char message[BUFFER_SIZE];
    while (TRUE) {
        if (fgets(message, sizeof(message), stdin) == NULL) break;

        // hacer que solo aparezca el mensaje prefijado por "[TÚ]"
        printf("\033[1A\r\033[K"); 
        fflush(stdout);

        int first_char = strspn(message, " \t\n\r");
        if (message[first_char] == '\0' || message[first_char] == '\n') {
            // mensaje vacío
            continue;
        }

        if (message[0] != '/') {
            char display[BUFFER_SIZE];
            strcpy(display, message);
            display[strcspn(display, "\n")] = '\0';   // remove trailing newline for display
            printf("[TÚ] %s\n", display);
        }
        if (send(sockfd, message, strlen(message), 0) < 0) {
#ifdef _WIN32
            fprintf(stderr, "send failed: %d\n", WSAGetLastError());
#else
            perror("send");
#endif
            break;
        }
    }

    CLOSE_SOCKET(sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}