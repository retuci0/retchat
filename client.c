#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define SERVER_PORT 8888

#define TRUE 1


int sockfd;

void *receive_messages(void *arg) {
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
        perror("recv");
    }

    close(sockfd);
    exit(0);
}

int main(int argc, char *argv[]) {
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

    struct sockaddr_in server_addr;
    pthread_t recv_thread;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        exit(1);
    }

    printf("conectado a %s:%d\n", server_ip, SERVER_PORT);
    printf("comandos: /nick <nombre>, /join <sala>\n");

    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("pthread_create");
        close(sockfd);
        exit(1);
    }

    char message[BUFFER_SIZE];
    while (TRUE) {
        if (fgets(message, sizeof(message), stdin) == NULL) break;
        if (send(sockfd, message, strlen(message), 0) < 0) {
            perror("send");
            break;
        }
    }

    close(sockfd);
    return 0;
}