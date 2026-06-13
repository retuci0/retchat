#define _GNU_SOURCE

#include "client.h"
#include "commands.h"
#include "dh.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/bn.h>
#include <openssl/hmac.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define HEARTBEAT_SEC 30


static int dh_handshake(int sock, uint8_t enc_key[KEY_LENGTH]) {
    BIGNUM* server_priv = BN_new();
    BIGNUM* server_pub  = BN_new();
    BIGNUM* client_pub  = BN_new();
    BIGNUM* shared      = BN_new();

    if (!server_priv || !server_pub || !client_pub || !shared) {
        BN_free(server_priv); BN_free(server_pub);
        BN_free(client_pub); BN_free(shared);
        return 0;
    }

    generate_private_key(server_priv);
    compute_public_key(server_priv, server_pub);

    size_t pub_len = BN_num_bytes(server_pub);
    uint8_t* pub_buf = malloc(pub_len);
    if (!pub_buf) goto error;
    BN_bn2bin(server_pub, pub_buf);
    uint32_t net_len = htonl(pub_len);

    if (send(sock, &net_len, 4, 0) != 4 ||
        send(sock, pub_buf, pub_len, 0) != (ssize_t) pub_len) {
        free(pub_buf);
        goto error;
    }
    free(pub_buf);

    if (recv(sock, &net_len, 4, 0) != 4) goto error;
    pub_len = ntohl(net_len);
    if (pub_len > 4096) goto error;  // sanity check
    pub_buf = malloc(pub_len);
    if (!pub_buf) goto error;
    if (recv(sock, pub_buf, pub_len, 0) != (ssize_t) pub_len) {
        free(pub_buf);
        goto error;
    }
    BN_bin2bn(pub_buf, pub_len, client_pub);
    free(pub_buf);

    compute_shared_secret(client_pub, server_priv, shared);
    derive_enc_key(shared, enc_key);

    BN_free(server_priv); BN_free(server_pub);
    BN_free(client_pub); BN_free(shared);
    return 1;

error:
    BN_free(server_priv); BN_free(server_pub);
    BN_free(client_pub); BN_free(shared);
    return 0;
}

void* client_handler(void* arg) {
    int client_sock = *((int*) arg);
    free(arg);

    struct timeval tv = { .tv_sec = HEARTBEAT_SEC, .tv_usec = 0 };
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t enc_key[KEY_LENGTH];
    if (!dh_handshake(client_sock, enc_key)) {
        close(client_sock);
        return NULL;
    }

    char default_name[NAME_LEN];
    snprintf(default_name, sizeof(default_name), "usuario%d", client_sock);

    client_t* new_client = malloc(sizeof(client_t));
    new_client->sockfd = client_sock;
    strncpy(new_client->name, default_name, NAME_LEN-1);
    new_client->name[NAME_LEN-1] = '\0';
    strncpy(new_client->room, "lobby", ROOM_LEN-1);
    new_client->room[ROOM_LEN-1] = '\0';
    memcpy(new_client->enc_key, enc_key, KEY_LENGTH);
    new_client->send_counter = 0;
    new_client->recv_counter = 0;
    new_client->next = NULL;

    pthread_mutex_lock(&clients_mutex);
    new_client->next = client_list;
    client_list = new_client;
    pthread_mutex_unlock(&clients_mutex);

    char welcome[BUFFER_SIZE];
    snprintf(welcome, sizeof(welcome), "[SERVER] buenas %s, estás en la sala \"%s\".\n",
             new_client->name, new_client->room);
    send_to_client(new_client, welcome);

    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg), "[SERVER] %s se ha unido.\n", new_client->name);
    broadcast_to_room(new_client->room, client_sock, join_msg, 1);

    uint8_t enc_buffer[MAX_MSG_LEN];
    uint8_t recv_hmac[32];

    while (1) {
        ssize_t hmac_read = recv(client_sock, recv_hmac, 32, 0);
        if (hmac_read <= 0) break;

        uint16_t net_msg_len;
        if (recv(client_sock, &net_msg_len, sizeof(net_msg_len), 0) != sizeof(net_msg_len)) break;
        uint16_t msg_len = ntohs(net_msg_len);
        if (msg_len > MAX_MSG_LEN) break;

        ssize_t total = 0;
        while (total < msg_len) {
            ssize_t r = recv(client_sock, enc_buffer + total, msg_len - total, 0);
            if (r <= 0) goto disconnect;
            total += r;
        }

        uint8_t expected_hmac[32];
        unsigned int hmac_len;
        HMAC(EVP_sha256(), new_client->enc_key, KEY_LENGTH,
             enc_buffer, msg_len, expected_hmac, &hmac_len);
        if (CRYPTO_memcmp(recv_hmac, expected_hmac, 32) != 0) continue;

        xor_crypt(enc_buffer, msg_len, new_client->enc_key, new_client->recv_counter);
        new_client->recv_counter++;
        enc_buffer[msg_len] = '\0';
        char* msg = (char*)enc_buffer;

        if (msg_len > 0 && msg[msg_len-1] == '\n')
            msg[msg_len-1] = '\0';

        if (!handle_command(new_client, msg)) {
            char out_msg[BUFFER_SIZE + NAME_LEN + 10];
            snprintf(out_msg, sizeof(out_msg), "[%s] %s\n", new_client->name, msg);
            broadcast_to_room(new_client->room, client_sock, out_msg, 1);
        }
    }

disconnect:
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

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    OpenSSL_add_all_algorithms();
    init_dh_params();

    int server_fd;
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
        int new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        printf("nueva conexión desde %s:%d\n",
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        int* pclient = malloc(sizeof(int));
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
    free_dh_params();
    return 0;
}