#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>            // para _beginthreadex
#define strcasecmp _stricmp
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// #define PORT 6677
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

/* MUTEX */
#ifdef _WIN32
static CRITICAL_SECTION clients_mutex;
#define MUTEX_INIT()    InitializeCriticalSection(&clients_mutex)
#define MUTEX_LOCK()    EnterCriticalSection(&clients_mutex)
#define MUTEX_UNLOCK()  LeaveCriticalSection(&clients_mutex)
#define MUTEX_DESTROY() DeleteCriticalSection(&clients_mutex)
#else
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
#define MUTEX_INIT()    (void)0
#define MUTEX_LOCK()    pthread_mutex_lock(&clients_mutex)
#define MUTEX_UNLOCK()  pthread_mutex_unlock(&clients_mutex)
#define MUTEX_DESTROY() pthread_mutex_destroy(&clients_mutex)
#endif


static client_t *client_list = NULL;

/* FUNCIONES HELPER */
void broadcast_to_room(const char *room, socket_t sender_sockfd, const char *msg, bool exclude_self) {
    MUTEX_LOCK();
    client_t *cur = client_list;
    while (cur) {
        if (strcmp(cur->room, room) == 0 &&
            (!exclude_self || cur->sockfd != sender_sockfd)) {
            if (send(cur->sockfd, msg, strlen(msg), 0) < 0) {
#ifdef _WIN32
                fprintf(stderr, "send error: %d\n", WSAGetLastError());
#else
                perror("send");
#endif
            }
        }
        cur = cur->next;
    }
    MUTEX_UNLOCK();
}

client_t *find_client(socket_t sockfd) {
    client_t *cur = client_list;
    while (cur) {
        if (cur->sockfd == sockfd) return cur;
        cur = cur->next;
    }
    return NULL;
}

void remove_client(socket_t sockfd) {
    client_t **cur = &client_list;
    while (*cur) {
        if ((*cur)->sockfd == sockfd) {
            client_t *to_delete = *cur;
            *cur = (*cur)->next;
#ifdef _WIN32
            closesocket(to_delete->sockfd);
#else
            close(to_delete->sockfd);
#endif
            free(to_delete);
            return;
        }
        cur = &(*cur)->next;
    }
}

bool is_username_taken(const char *username, const char *room, socket_t exclude_sockfd) {
    client_t *cur = client_list;
    while (cur) {
        if (cur->sockfd != exclude_sockfd &&
            strcmp(cur->name, username) == 0 &&
            strcmp(cur->room, room) == 0) {
            return true;
        }
        cur = cur->next;
    }
    return false;
}

static bool is_forbidden_name(const char *name) {
    return (strcasecmp(name, "TÚ") == 0);
}

/* HILO: gestiona un cliente */
#ifdef _WIN32
static unsigned __stdcall client_handler(void *arg)
#else
static void *client_handler(void *arg)
#endif
{
    socket_t client_sock = *(socket_t*)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes_read;

    // sala y nombre por defecto
    char default_name[NAME_LEN];
#ifdef _WIN32
    snprintf(default_name, sizeof(default_name), "usuario%lld", (long long)client_sock);
#else
    snprintf(default_name, sizeof(default_name), "usuario%d", client_sock);
#endif
    char default_room[ROOM_LEN] = "lobby";

    client_t *new_client = malloc(sizeof(client_t));
    new_client->sockfd = client_sock;
    strncpy(new_client->name, default_name, NAME_LEN-1);
    new_client->name[NAME_LEN-1] = '\0';
    strncpy(new_client->room, default_room, ROOM_LEN-1);
    new_client->room[ROOM_LEN-1] = '\0';
    new_client->next = NULL;

    MUTEX_LOCK();
    new_client->next = client_list;
    client_list = new_client;
    MUTEX_UNLOCK();

    // dar la bienvenida
    char welcome[BUFFER_SIZE];
    snprintf(welcome, sizeof(welcome), "buenas %s, estás en la sala '%s'.\n",
             new_client->name, new_client->room);
    send(client_sock, welcome, strlen(welcome), 0);

    // notificar sala del nuevo usuario
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg), "[SERVER] %s se ha unido.\n", new_client->name);
    broadcast_to_room(new_client->room, client_sock, join_msg, true);

    while ((bytes_read = recv(client_sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[bytes_read] = '\0';

        if (buffer[bytes_read-1] == '\n')
            buffer[bytes_read-1] = '\0';

        /* COMANDOS */
        if (buffer[0] == '/') {
            if (strncmp(buffer, "/nick ", 6) == 0) {
                char newname[NAME_LEN];
                sscanf(buffer + 6, "%31s", newname);

                if (is_forbidden_name(newname)) {
                    send(client_sock, "[SERVER] 'buen intento.\n", 47, 0);
                    continue;
                }

                if (strcmp(newname, new_client->name) == 0) {
                    send(client_sock, "[SERVER] ya tienes ese nombre.\n", 37, 0);
                    continue;
                }

                MUTEX_LOCK();
                if (is_username_taken(newname, new_client->room, client_sock)) {
                    MUTEX_UNLOCK();
                    char err[BUFFER_SIZE];
                    snprintf(err, sizeof(err), "[SERVER] ya hay un '%s' en esta sala.\n", newname);
                    send(client_sock, err, strlen(err), 0);
                    continue;
                }

                char old_name[NAME_LEN];
                strcpy(old_name, new_client->name);

                strncpy(new_client->name, newname, NAME_LEN-1);
                new_client->name[NAME_LEN-1] = '\0';
                MUTEX_UNLOCK();

                char ack[BUFFER_SIZE];
                snprintf(ack, sizeof(ack), "[SERVER] ahora eres %s.\n", new_client->name);
                send(client_sock, ack, strlen(ack), 0);

                char namechange_msg[BUFFER_SIZE];
                snprintf(namechange_msg, sizeof(namechange_msg), "[SERVER] %s ahora es %s.\n", old_name, new_client->name);
                broadcast_to_room(new_client->room, client_sock, namechange_msg, true);
            }
            else if (strncmp(buffer, "/join ", 6) == 0) {
                char newroom[ROOM_LEN];
                sscanf(buffer + 6, "%31s", newroom);

                if (strcmp(newroom, new_client->room) == 0) {
                    send(client_sock, "[SERVER] ya estás en esa sala.\n", 39, 0);
                    continue;
                }

                MUTEX_LOCK();
                if (is_username_taken(new_client->name, newroom, -1)) {
                    MUTEX_UNLOCK();
                    char err[BUFFER_SIZE];
                    snprintf(err, sizeof(err), "[SERVER] tu nombre '%s' ya está cogido por alguien en esa sala '%s'.\n",
                             new_client->name, newroom);
                    send(client_sock, err, strlen(err), 0);
                    continue;
                }

                // anunciar pirada
                char leave_msg[BUFFER_SIZE];
                snprintf(leave_msg, sizeof(leave_msg), "[SERVER] %s se ha pirado.\n", new_client->name);
                MUTEX_UNLOCK();
                broadcast_to_room(new_client->room, client_sock, leave_msg, true);

                MUTEX_LOCK();
                strncpy(new_client->room, newroom, ROOM_LEN-1);
                new_client->room[ROOM_LEN-1] = '\0';
                MUTEX_UNLOCK();

                char join_msg[BUFFER_SIZE];
                snprintf(join_msg, sizeof(join_msg), "[SERVER] %s se ha unido.\n", new_client->name);
                broadcast_to_room(new_client->room, client_sock, join_msg, true);

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
            // mensaje normal
            char out_msg[BUFFER_SIZE + NAME_LEN + 10];
            snprintf(out_msg, sizeof(out_msg), "[%s] %s\n", new_client->name, buffer);
            broadcast_to_room(new_client->room, client_sock, out_msg, true);
        }
    }

    // cliente desconectado
    if (bytes_read == 0) {
#ifdef _WIN32
        printf("cliente %lld desconectado.\n", (long long)client_sock);
#else
        printf("cliente %d desconectado.\n", client_sock);
#endif
    } else {
#ifdef _WIN32
        fprintf(stderr, "recv error: %d\n", WSAGetLastError());
#else
        perror("recv");
#endif
    }

    MUTEX_LOCK();
    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, sizeof(leave_msg), "[SERVER] %s se ha pirado.\n", new_client->name);
    char old_room[ROOM_LEN];
    strcpy(old_room, new_client->room);
    remove_client(client_sock);
    MUTEX_UNLOCK();

    broadcast_to_room(old_room, -1, leave_msg, false);

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* MAIN ENTRYPOINT */
int main(int argc, char *argv[]) {
    const int port = (argc > 1) ? atoi(argv[1]) : 6677;

    socket_t server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        exit(1);
    }
#endif

    MUTEX_INIT();

    // crear socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) ==
#ifdef _WIN32
        INVALID_SOCKET
#else
        -1
#endif
    ) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // permitir reusar dirección
#ifdef _WIN32
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) {
#else
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
#endif
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("server escuchando por puerto %d\n", port);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
#ifdef _WIN32
        if (new_socket == INVALID_SOCKET) {
#else
        if (new_socket < 0) {
#endif
            perror("accept");
            continue;
        }

        printf("nueva conexión, socket fd: %lld, ip: %s, puerto: %d\n",
               (long long) new_socket,
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        // alocar memoria y crear hilo
        socket_t *pclient = malloc(sizeof(socket_t));
        *pclient = new_socket;
#ifdef _WIN32
        HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, client_handler, pclient, 0, NULL);
        if (hThread == NULL) {
            perror("_beginthreadex");
            closesocket(new_socket);
            free(pclient);
        } else {
            CloseHandle(hThread);   // desadjuntar
        }
#else
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, pclient) != 0) {
            perror("pthread_create");
            close(new_socket);
            free(pclient);
        } else {
            pthread_detach(thread_id);
        }
#endif
    }
#ifdef _WIN32
    closesocket(server_fd);
#else
    close(server_fd);
#endif
    MUTEX_DESTROY();
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}