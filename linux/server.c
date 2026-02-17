#define _GNU_SOURCE
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
#include <strings.h>

#define MAX_CLIENTS 100

typedef struct client {
    int sockfd;
    char name[NAME_LEN];
    char room[ROOM_LEN];
    uint8_t enc_key[KEY_LENGTH];
    struct client *next;
} client_t;

static client_t *client_list = NULL;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/* FUNCIONES HELPER */

/* enviar un mensaje a un cliente */
void send_to_client(client_t *client, const char *msg) {
    size_t len = strlen(msg) + 1;
    uint8_t *enc_buf = malloc(len);
    memcpy(enc_buf, msg, len);
    xor_crypt(enc_buf, len, client->enc_key);
    send(client->sockfd, enc_buf, len, 0);
    free(enc_buf);
}

/* enviar un mensaje a todos los clientes en una sala */
void broadcast_to_room(const char *room, int sender_sockfd, const char *msg, int exclude_self) {
    pthread_mutex_lock(&clients_mutex);
    client_t *cur = client_list;
    while (cur) {
        if (strcmp(cur->room, room) == 0 &&
            (!exclude_self || cur->sockfd != sender_sockfd)) {
            send_to_client(cur, msg);
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

/* eliminar un cliente de una sala */
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

int is_username_taken(const char *username, const char *room, int exclude_sockfd) {
    client_t *cur = client_list;
    while (cur) {
        if (cur->sockfd != exclude_sockfd &&
            strcmp(cur->name, username) == 0 &&
            strcmp(cur->room, room) == 0) {
            return 1;
        }
        cur = cur->next;
    }
    return 0;
}

int is_forbidden_name(const char *name) {
    return (strcasecmp(name, "TÚ") == 0)
    || (strcasecmp(name, "SERVER") == 0);
}

/* hilo gestor de cliente */
void *client_handler(void *arg) {
    int client_sock = *(int*)arg;
    free(arg);

    // intercambio de claves
    uint64_t server_priv = generate_private_key();
    uint64_t server_pub = compute_public_key(server_priv);
    uint64_t client_pub;

    if (send(client_sock, &server_pub, sizeof(server_pub), 0) != sizeof(server_pub)) {
        close(client_sock);
        return NULL;
    }
    if (recv(client_sock, &client_pub, sizeof(client_pub), 0) != sizeof(client_pub)) {
        close(client_sock);
        return NULL;
    }

    uint64_t shared_secret = compute_shared_secret(client_pub, server_priv);
    uint8_t enc_key[KEY_LENGTH];
    kdf_derive(shared_secret, enc_key, KEY_LENGTH);

    // crear cliente
    char default_name[NAME_LEN];
    snprintf(default_name, sizeof(default_name), "usuario%d", client_sock);

    client_t *new_client = malloc(sizeof(client_t));
    new_client->sockfd = client_sock;
    strncpy(new_client->name, default_name, NAME_LEN-1);
    new_client->name[NAME_LEN-1] = '\0';
    strncpy(new_client->room, "lobby", ROOM_LEN-1);
    new_client->room[ROOM_LEN-1] = '\0';
    memcpy(new_client->enc_key, enc_key, KEY_LENGTH);
    new_client->next = NULL;

    pthread_mutex_lock(&clients_mutex);
    new_client->next = client_list;
    client_list = new_client;
    pthread_mutex_unlock(&clients_mutex);

    // dar bienvenida
    char welcome[BUFFER_SIZE];
    snprintf(welcome, sizeof(welcome), "buenas %s, estás en la sala '%s'.\n",
             new_client->name, new_client->room);
    send_to_client(new_client, welcome);

    // notificar a la sala del nuevo usuario
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg), "[SERVER] %s se ha unido.\n", new_client->name);
    broadcast_to_room(new_client->room, client_sock, join_msg, 1);

    // bucle principal de recepción
    uint8_t enc_buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(client_sock, enc_buffer, sizeof(enc_buffer)-1, 0)) > 0) {
        xor_crypt(enc_buffer, bytes_read, enc_key);
        enc_buffer[bytes_read] = '\0';
        char *msg = (char*)enc_buffer;

        if (bytes_read > 0 && msg[bytes_read-1] == '\n')
            msg[bytes_read-1] = '\0';

        if (msg[0] == '/') {
            // cambiar el nombre del usuario con /nick
            if (strncmp(msg, "/nick ", 6) == 0) {
                char newname[NAME_LEN];
                sscanf(msg + 6, "%31s", newname);

                if (is_forbidden_name(newname)) {
                    send_to_client(new_client, "[SERVER] buen intento.\n");
                    continue;
                }
                if (strcmp(newname, new_client->name) == 0) {
                    send_to_client(new_client, "[SERVER] ya tienes ese nombre.\n");
                    continue;
                }

                pthread_mutex_lock(&clients_mutex);
                if (is_username_taken(newname, new_client->room, client_sock)) {
                    pthread_mutex_unlock(&clients_mutex);
                    char err[BUFFER_SIZE];
                    snprintf(err, sizeof(err), "[SERVER] el nombre '%s' ya está en uso en esta sala.\n", newname);
                    send_to_client(new_client, err);
                    continue;
                }

                char old_name[NAME_LEN];
                strcpy(old_name, new_client->name);
                strncpy(new_client->name, newname, NAME_LEN-1);
                new_client->name[NAME_LEN-1] = '\0';
                pthread_mutex_unlock(&clients_mutex);

                char ack[BUFFER_SIZE];
                snprintf(ack, sizeof(ack), "[SERVER] ahora eres %s.\n", new_client->name);
                send_to_client(new_client, ack);

                char namechange_msg[BUFFER_SIZE];
                snprintf(namechange_msg, sizeof(namechange_msg), "[SERVER] %s ahora es %s.\n", old_name, new_client->name);
                broadcast_to_room(new_client->room, client_sock, namechange_msg, 1);
            }

            // unirse a una sala con /join
            else if (strncmp(msg, "/join ", 6) == 0) {
                char newroom[ROOM_LEN];
                sscanf(msg + 6, "%31s", newroom);

                if (strcmp(newroom, new_client->room) == 0) {
                    send_to_client(new_client, "[SERVER] ya estás en esa sala.\n");
                    continue;
                }

                pthread_mutex_lock(&clients_mutex);
                if (is_username_taken(new_client->name, newroom, -1)) {
                    pthread_mutex_unlock(&clients_mutex);
                    char err[BUFFER_SIZE];
                    snprintf(err, sizeof(err), "[SERVER] tu nombre '%s' ya está cogido en la sala '%s'.\n",
                             new_client->name, newroom);
                    send_to_client(new_client, err);
                    continue;
                }

                // anunciar salida de la sala anterior
                char leave_msg[BUFFER_SIZE];
                snprintf(leave_msg, sizeof(leave_msg), "[SERVER] %s se ha pirado.\n", new_client->name);
                pthread_mutex_unlock(&clients_mutex);  // liberar antes de broadcast para evitar deadlock
                broadcast_to_room(new_client->room, client_sock, leave_msg, 1);

                pthread_mutex_lock(&clients_mutex);
                strncpy(new_client->room, newroom, ROOM_LEN-1);
                new_client->room[ROOM_LEN-1] = '\0';
                pthread_mutex_unlock(&clients_mutex);

                // anunciar entrada a la nueva sala
                char join_msg[BUFFER_SIZE];
                snprintf(join_msg, sizeof(join_msg), "[SERVER] %s se ha unido.\n", new_client->name);
                broadcast_to_room(new_client->room, client_sock, join_msg, 1);

                char ack[BUFFER_SIZE];
                snprintf(ack, sizeof(ack), "[SERVER] ahora estás en la sala '%s'.\n", new_client->room);
                send_to_client(new_client, ack);
            }
            else {
                send_to_client(new_client, "[SERVER] comando desconocido.\n");
            }
        }
        else {
            // mensaje normal
            char out_msg[BUFFER_SIZE + NAME_LEN + 10];
            snprintf(out_msg, sizeof(out_msg), "[%s] %s\n", new_client->name, msg);
            broadcast_to_room(new_client->room, client_sock, out_msg, 1);
        }
    }

    // cliente desconectado
    printf("cliente %d desconectado.\n", client_sock);

    pthread_mutex_lock(&clients_mutex);
    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, sizeof(leave_msg), "[SERVER] %s se ha pirado.\n", new_client->name);
    char old_room[ROOM_LEN];
    strcpy(old_room, new_client->room);
    remove_client(client_sock);
    pthread_mutex_unlock(&clients_mutex);

    broadcast_to_room(old_room, -1, leave_msg, 0);

    return NULL;
}

/* ENTRYPOINT PRINCIPAL */
int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("server escuchando en puerto %d\n", port);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        printf("nueva conexión desde %s:%d\n",
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));

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
