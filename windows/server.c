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

#define MAX_CLIENTS 100

typedef struct client {
    SOCKET sockfd;
    char name[NAME_LEN];
    char room[ROOM_LEN];
    uint8_t enc_key[KEY_LENGTH];
    struct client *next;
} client_t;

static client_t *client_list = NULL;
static CRITICAL_SECTION clients_mutex;

/* FUNCIONES HELPER */

/* enviar mensaje a cliente */
void send_to_client(client_t *client, const char *msg) {
    size_t len = strlen(msg) + 1;
    uint8_t *enc_buf = malloc(len);
    memcpy(enc_buf, msg, len);
    xor_crypt(enc_buf, len, client->enc_key);
    send(client->sockfd, (char*) enc_buf, (int) len, 0);
    free(enc_buf);
}

/* enviar mensaje a todos los clientes en una sala */
void broadcast_to_room(const char *room, SOCKET sender_sockfd, const char *msg, BOOL exclude_self) {
    EnterCriticalSection(&clients_mutex);
    client_t *cur = client_list;
    while (cur) {
        if (strcmp(cur->room, room) == 0 &&
            (!exclude_self || cur->sockfd != sender_sockfd)) {
            send_to_client(cur, msg);
        }
        cur = cur->next;
    }
    LeaveCriticalSection(&clients_mutex);
}

/* eliminar cliente */
void remove_client(SOCKET sockfd) {
    client_t **cur = &client_list;
    while (*cur) {
        if ((*cur)->sockfd == sockfd) {
            client_t *to_delete = *cur;
            *cur = (*cur)->next;
            closesocket(to_delete->sockfd);
            free(to_delete);
            return;
        }
        cur = &(*cur)->next;
    }
}

BOOL is_username_taken(const char *username, const char *room, SOCKET exclude_sockfd) {
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

BOOL is_forbidden_name(const char *name) {
    // usar _stricmp en lugar de strcasecmp
    return (_stricmp(name, "TÚ") == 0);
}

// HILO RECEPTOR
unsigned __stdcall client_handler(void *arg) {
    SOCKET client_sock = *(SOCKET*)arg;
    free(arg);

    // intercambio de claves
    uint64_t server_priv = generate_private_key();
    uint64_t server_pub = compute_public_key(server_priv);
    uint64_t client_pub;

    if (send(client_sock, (char*)&server_pub, sizeof(server_pub), 0) != sizeof(server_pub)) {
        closesocket(client_sock);
        return 0;
    }
    if (recv(client_sock, (char*)&client_pub, sizeof(client_pub), 0) != sizeof(client_pub)) {
        closesocket(client_sock);
        return 0;
    }

    uint64_t shared_secret = compute_shared_secret(client_pub, server_priv);
    uint8_t enc_key[KEY_LENGTH];
    kdf_derive(shared_secret, enc_key, KEY_LENGTH);

    // crear cliente
    char default_name[NAME_LEN];
    snprintf(default_name, sizeof(default_name), "usuario%lld", (long long)client_sock);

    client_t *new_client = malloc(sizeof(client_t));
    new_client->sockfd = client_sock;
    strncpy(new_client->name, default_name, NAME_LEN-1);
    new_client->name[NAME_LEN-1] = '\0';
    strncpy(new_client->room, "lobby", ROOM_LEN-1);
    new_client->room[ROOM_LEN-1] = '\0';
    memcpy(new_client->enc_key, enc_key, KEY_LENGTH);
    new_client->next = NULL;

    EnterCriticalSection(&clients_mutex);
    new_client->next = client_list;
    client_list = new_client;
    LeaveCriticalSection(&clients_mutex);

    // dar bienvenida
    char welcome[BUFFER_SIZE];
    snprintf(welcome, sizeof(welcome), "buenas %s, estás en la sala '%s'.\n",
             new_client->name, new_client->room);
    send_to_client(new_client, welcome);

    // notificar a la sala del nuevo usuario
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg), "[SERVER] %s se ha unido.\n", new_client->name);
    broadcast_to_room(new_client->room, client_sock, join_msg, TRUE);

    // bucle principal de recepción
    uint8_t enc_buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(client_sock, (char*)enc_buffer, sizeof(enc_buffer)-1, 0)) > 0) {
        xor_crypt(enc_buffer, bytes_read, enc_key);
        enc_buffer[bytes_read] = '\0';
        char *msg = (char*)enc_buffer;

        if (bytes_read > 0 && msg[bytes_read-1] == '\n')
            msg[bytes_read-1] = '\0';

        if (msg[0] == '/') {
            // usar /nick para cambiar el nombre
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

                EnterCriticalSection(&clients_mutex);
                if (is_username_taken(newname, new_client->room, client_sock)) {
                    LeaveCriticalSection(&clients_mutex);
                    char err[BUFFER_SIZE];
                    snprintf(err, sizeof(err), "[SERVER] ya hay un '%s' en esta sala.\n", newname);
                    send_to_client(new_client, err);
                    continue;
                }

                char old_name[NAME_LEN];
                strcpy(old_name, new_client->name);
                strncpy(new_client->name, newname, NAME_LEN-1);
                new_client->name[NAME_LEN-1] = '\0';
                LeaveCriticalSection(&clients_mutex);

                char ack[BUFFER_SIZE];
                snprintf(ack, sizeof(ack), "[SERVER] ahora eres %s.\n", new_client->name);
                send_to_client(new_client, ack);

                char namechange_msg[BUFFER_SIZE];
                snprintf(namechange_msg, sizeof(namechange_msg), "[SERVER] %s ahora es %s.\n", old_name, new_client->name);
                broadcast_to_room(new_client->room, client_sock, namechange_msg, TRUE);
            }

            // usar /join para unirse a una sala
            else if (strncmp(msg, "/join ", 6) == 0) {
                char newroom[ROOM_LEN];
                sscanf(msg + 6, "%31s", newroom);

                if (strcmp(newroom, new_client->room) == 0) {
                    send_to_client(new_client, "[SERVER] ya estás en esa sala.\n");
                    continue;
                }

                EnterCriticalSection(&clients_mutex);
                if (is_username_taken(new_client->name, newroom, -1)) {
                    LeaveCriticalSection(&clients_mutex);
                    char err[BUFFER_SIZE];
                    snprintf(err, sizeof(err), "[SERVER] ya hay un '%s' en esa sala '%s'.\n",
                             new_client->name, newroom);
                    send_to_client(new_client, err);
                    continue;
                }

                char leave_msg[BUFFER_SIZE];
                snprintf(leave_msg, sizeof(leave_msg), "[SERVER] %s se ha pirado.\n", new_client->name);
                LeaveCriticalSection(&clients_mutex);
                broadcast_to_room(new_client->room, client_sock, leave_msg, TRUE);

                EnterCriticalSection(&clients_mutex);
                strncpy(new_client->room, newroom, ROOM_LEN-1);
                new_client->room[ROOM_LEN-1] = '\0';
                LeaveCriticalSection(&clients_mutex);

                char join_msg[BUFFER_SIZE];
                snprintf(join_msg, sizeof(join_msg), "[SERVER] %s se ha unido.\n", new_client->name);
                broadcast_to_room(new_client->room, client_sock, join_msg, TRUE);

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
            broadcast_to_room(new_client->room, client_sock, out_msg, TRUE);
        }
    }

    // cliente desconectado
    printf("cliente %lld desconectado.\n", (long long) client_sock);

    EnterCriticalSection(&clients_mutex);
    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, sizeof(leave_msg), "[SERVER] %s se ha pirado.\n", new_client->name);
    char old_room[ROOM_LEN];
    strcpy(old_room, new_client->room);
    remove_client(client_sock);
    LeaveCriticalSection(&clients_mutex);

    broadcast_to_room(old_room, -1, leave_msg, FALSE);

    return 0;
}

/* ENTRYPOINT PRINCIPAL */
int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    WSADATA wsaData;
    SOCKET server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    InitializeCriticalSection(&clients_mutex);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("socket failed\n");
        WSACleanup();
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("bind failed\n");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) == SOCKET_ERROR) {
        printf("listen failed\n");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    printf("servidor escuchando en puerto %d\n", port);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket == INVALID_SOCKET) {
            printf("accept failed\n");
            continue;
        }

        printf("nueva conexión desde %s:%d\n",
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        SOCKET *pclient = malloc(sizeof(SOCKET));
        *pclient = new_socket;

        HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, client_handler, pclient, 0, NULL);
        if (hThread == NULL) {
            printf("error creating thread\n");
            closesocket(new_socket);
            free(pclient);
        } else {
            CloseHandle(hThread);
        }
    }

    closesocket(server_fd);
    DeleteCriticalSection(&clients_mutex);
    WSACleanup();
    return 0;
}
