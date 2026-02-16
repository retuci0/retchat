#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>

// compat. con Windows
#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

#define NAME_LEN 32
#define ROOM_LEN 32

typedef struct client {
    socket_t sockfd;
    char name[NAME_LEN];
    char room[ROOM_LEN];
    struct client *next;
} client_t;

void broadcast_to_room(const char *room, socket_t sender_sockfd, const char *msg, bool exclude_self);
client_t *find_client(socket_t sockfd);
void remove_client(socket_t sockfd);
bool is_username_taken(const char *username, const char *room, socket_t exclude_sockfd);
static bool is_forbidden_name(const char *name);

#endif