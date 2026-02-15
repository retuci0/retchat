#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8888
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define NAME_LEN 32
#define ROOM_LEN 32

#define TRUE 1
#define FALSE 0


// cliente
typedef struct client {
    int sockfd;
    char name[NAME_LEN];
    char room[ROOM_LEN];
    struct client *next;
} client_t;

client_t *client_list = NULL;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

client_t* find_client(int sockfd) {
    client_t *cur = client_list;
    while (cur) {
        if (cur->sockfd == sockfd) return cur;
        cur = cur->next;
    }
    return NULL;
}

int is_username_taken(const char *username, const char *room, int exclude_sockfd) {
    client_t *cur = client_list;
    while (cur) {
        if (cur->sockfd != exclude_sockfd &&
            strcmp(cur->name, username) == 0 &&
            strcmp(cur->room, room) == 0) {
            return TRUE;
        }
        cur = cur->next;
    }
    return FALSE;
}

void remove_client(int sockfd) {
    client_t **cur = &client_list;
    while (*cur) {
        if ((*cur)->sockfd == sockfd) {
            client_t *to_delete = *cur;
            *cur = (*cur)->next;
            close(to_delete->sockfd);
            free(to_delete);
            return;
        }
        cur = &(*cur)->next;
    }
}

// mandar mensaje a todos los usuarios de la sala, excepto a uno mismo si exclude_self es TRUE
void broadcast_to_room(const char *room, int sender_sockfd, const char *msg, int exclude_self) {
    pthread_mutex_lock(&clients_mutex);
    client_t *cur = client_list;
    while (cur) {
        if (strcmp(cur->room, room) == 0 &&
            (!exclude_self || cur->sockfd != sender_sockfd)) {
            if (send(cur->sockfd, msg, strlen(msg), 0) < 0) {
                perror("send");
            }
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *client_handler(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes_read;

    // nombre y sala por defecto
    char default_name[NAME_LEN];
    snprintf(default_name, sizeof(default_name), "User%d", client_sock);
    char default_room[ROOM_LEN] = "Lobby";

    client_t *new_client = malloc(sizeof(client_t));
    new_client->sockfd = client_sock;
    strncpy(new_client->name, default_name, NAME_LEN-1);
    new_client->name[NAME_LEN-1] = '\0';
    strncpy(new_client->room, default_room, ROOM_LEN-1);
    new_client->room[ROOM_LEN-1] = '\0';
    new_client->next = NULL;

    pthread_mutex_lock(&clients_mutex);
    new_client->next = client_list;
    client_list = new_client;
    pthread_mutex_unlock(&clients_mutex);

    // dar la bienvenida
    char welcome[BUFFER_SIZE];
    snprintf(welcome, sizeof(welcome), "buenas %s, estás en la sala '%s'.\n",
             new_client->name, new_client->room);
    send(client_sock, welcome, strlen(welcome), 0);

    // notificar sala de que hay un nuevo usuario
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg), "[SERVER] %s se ha unido.\n", new_client->name);
    broadcast_to_room(new_client->room, client_sock, join_msg, 1);

    while ((bytes_read = recv(client_sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[bytes_read] = '\0';

        if (buffer[bytes_read-1] == '\n')
            buffer[bytes_read-1] = '\0';

        if (buffer[0] == '/') {
            if (strncmp(buffer, "/nick ", 6) == 0) {
                char newname[NAME_LEN];
                sscanf(buffer + 6, "%31s", newname);

                if (strcmp(newname, new_client->name) == 0) {
                    send(client_sock, "[SERVER] ya tienes ese nombre.\n", 37, 0);
                    continue;
                }

                pthread_mutex_lock(&clients_mutex);
                if (is_username_taken(newname, new_client->room, client_sock)) {
                    pthread_mutex_unlock(&clients_mutex);
                    char err[BUFFER_SIZE];
                    snprintf(err, sizeof(err), "[SERVER] '%s' ya es un usuario en esta sala.\n", newname);
                    send(client_sock, err, strlen(err), 0);
                    continue;
                }

                char old_name[NAME_LEN];
                strcpy(old_name, new_client->name);

                strncpy(new_client->name, newname, NAME_LEN-1);
                new_client->name[NAME_LEN-1] = '\0';
                pthread_mutex_unlock(&clients_mutex);

                char ack[BUFFER_SIZE];
                snprintf(ack, sizeof(ack), "[SERVER] ahora eres %s.\n", new_client->name);
                send(client_sock, ack, strlen(ack), 0);

                char namechange_msg[BUFFER_SIZE];
                snprintf(namechange_msg, sizeof(namechange_msg), "[SERVER] %s ahora es %s.\n", old_name, new_client->name);
                broadcast_to_room(new_client->room, client_sock, namechange_msg, 1);
            }
            else if (strncmp(buffer, "/join ", 6) == 0) {
                char newroom[ROOM_LEN];
                sscanf(buffer + 6, "%31s", newroom);

                if (strcmp(newroom, new_client->room) == 0) {
                    send(client_sock, "[SERVER] ya estás en esa sala.\n", 39, 0);
                    continue;
                }

                pthread_mutex_lock(&clients_mutex);
                if (is_username_taken(new_client->name, newroom, -1)) {
                    pthread_mutex_unlock(&clients_mutex);
                    char err[BUFFER_SIZE];
                    snprintf(err, sizeof(err), "[SERVER] tu nombre '%s'ya está cogido en esa sala '%s'.\n",
                             new_client->name, newroom);
                    send(client_sock, err, strlen(err), 0);
                    continue;
                }

                char leave_msg[BUFFER_SIZE];
                snprintf(leave_msg, sizeof(leave_msg), "[SERVER] %s se ha pirado.\n", new_client->name);
                pthread_mutex_unlock(&clients_mutex);
                broadcast_to_room(new_client->room, client_sock, leave_msg, 1);

                pthread_mutex_lock(&clients_mutex);
                strncpy(new_client->room, newroom, ROOM_LEN-1);
                new_client->room[ROOM_LEN-1] = '\0';
                pthread_mutex_unlock(&clients_mutex);

                char join_msg[BUFFER_SIZE];
                snprintf(join_msg, sizeof(join_msg), "[SERVER] %s se ha unido.\n", new_client->name);
                broadcast_to_room(new_client->room, client_sock, join_msg, 1);

                char ack[BUFFER_SIZE];
                snprintf(ack, sizeof(ack), "[SERVER] ahora estás en la sala '%s'.\n", new_client->room);
                send(client_sock, ack, strlen(ack), 0);
            }
            else {
                char *err = "[SERVER] comando desconocido.\n";
                send(client_sock, err, strlen(err), 0);
            }
        }
        else {
            char out_msg[BUFFER_SIZE + NAME_LEN + 10];
            snprintf(out_msg, sizeof(out_msg), "[%s] %s\n", new_client->name, buffer);
            broadcast_to_room(new_client->room, client_sock, out_msg, 1);
        }
    }

    if (bytes_read == 0) {
        printf("cliente %d desconectado.\n", client_sock);
    } else {
        perror("recv");
    }

    pthread_mutex_lock(&clients_mutex);
    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, sizeof(leave_msg), "[SERVER] %s se ha pirado.\n", new_client->name);
    char old_room[ROOM_LEN];
    strcpy(old_room, new_client->room);
    remove_client(client_sock);
    pthread_mutex_unlock(&clients_mutex);

    broadcast_to_room(old_room, -1, leave_msg, FALSE);

    return NULL;
}

int main(int argc, char *argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // reutilizar dirección
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bindear
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // escuchar
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("server escuchando por puerto %d\n", PORT);

    while (TRUE) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        printf("nueva conexión, socket fd: %d, ip: %s, puerto: %d\n",
               new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        // alocar memoria y crear hilo
        int *pclient = malloc(sizeof(int));
        *pclient = new_socket;
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, pclient) != 0) {
            perror("pthread_create");
            close(new_socket);
            free(pclient);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(server_fd);
    return 0;
}