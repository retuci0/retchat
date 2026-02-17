#include "../common/protocol.h"
#include "../common/crypto.h"
#include "../common/kdf.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// widgets
static GtkWidget *window;
static GtkWidget *text_view;
static GtkWidget *entry;

// estado de la conexión
static int sockfd;
static uint8_t enc_key[KEY_LENGTH];
static char current_room[ROOM_LEN] = "lobby";
static char current_nick[NAME_LEN] = "usuario?";
static char server_ip_str[64];
static int server_port;

// estructura para pasar mensajes al hilo principal desde el hilo receptor
typedef struct {
    char msg[BUFFER_SIZE];
} MessageData;

// para añadir texto al TextView
static gboolean idle_append_message(gpointer data) {
    MessageData *msg_data = (MessageData*)data;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, msg_data->msg, -1);
    // autoscroll al final
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(text_view), &end, 0.0, FALSE, 0.0, 0.0);
    g_free(msg_data);
    return FALSE; // eliminar de la cola
}

// para actualizar el título de la ventana
static gboolean idle_update_title(gpointer data) {
    char title[256];
    snprintf(title, sizeof(title), "retchat - %s@%s - %s:%d",
             current_nick, current_room, server_ip_str, server_port);
    gtk_window_set_title(GTK_WINDOW(window), title);
    return FALSE;
}

// recibir mensajes del servidor
static void *receive_messages(void *arg) {
    uint8_t enc_buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(sockfd, enc_buffer, sizeof(enc_buffer) - 1, 0)) > 0) {
        xor_crypt(enc_buffer, bytes_received, enc_key);
        enc_buffer[bytes_received] = '\0';
        char *decrypted = (char*)enc_buffer;

        // detectar cambios de nombre y sala
        if (strncmp(decrypted, "[SERVER] ahora eres ", 20) == 0) {
            char newname[NAME_LEN];
            if (sscanf(decrypted + 20, "%31[^.]", newname) == 1) {
                strncpy(current_nick, newname, NAME_LEN - 1);
                current_nick[NAME_LEN - 1] = '\0';
                g_idle_add(idle_update_title, NULL);
            }
        } else if (strncmp(decrypted, "[SERVER] ahora estás en la sala '", 34) == 0) {
            char newroom[ROOM_LEN];
            if (sscanf(decrypted + 34, "%31[^']", newroom) == 1) {
                strncpy(current_room, newroom, ROOM_LEN - 1);
                current_room[ROOM_LEN - 1] = '\0';
                g_idle_add(idle_update_title, NULL);
            }
        }

        // pasar el mensaje al hilo principal para mostrarlo en el TextView
        MessageData *msg_data = g_new(MessageData, 1);
        strncpy(msg_data->msg, decrypted, BUFFER_SIZE - 1);
        msg_data->msg[BUFFER_SIZE - 1] = '\0';
        g_idle_add(idle_append_message, msg_data);
    }

    // desconexión
    MessageData *disc_msg = g_new(MessageData, 1);
    snprintf(disc_msg->msg, sizeof(disc_msg->msg), "\ndesconectado del servidor.\n");
    g_idle_add(idle_append_message, disc_msg);

    close(sockfd);
    gtk_main_quit();  // salir
    return NULL;
}

// callback para el botón de enviar / tecla Enter
static void on_send_clicked(GtkWidget *widget, gpointer data) {
    const char *input = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(input) == 0) return;

    // mostrar eco local en el TextView
    if (input[0] != '/') {
        char local_display[BUFFER_SIZE + 10];
        snprintf(local_display, sizeof(local_display), "[TÚ] %s\n", input);
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(buffer, &end);
        gtk_text_buffer_insert(buffer, &end, local_display, -1);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(text_view), &end, 0.0, FALSE, 0.0, 0.0);
    }

    // encriptar y enviar
    size_t msg_len = strlen(input) + 1;  // incluir el null terminator
    uint8_t *enc_msg = malloc(msg_len);
    memcpy(enc_msg, input, msg_len);
    xor_crypt(enc_msg, msg_len, enc_key);

    if (send(sockfd, enc_msg, msg_len, 0) < 0) {
        perror("send");
        free(enc_msg);
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "error al enviar el mensaje"
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    } else {
        free(enc_msg);
    }

    gtk_entry_set_text(GTK_ENTRY(entry), "");
}

// inicializar la ventana principal
static void activate(GtkApplication *app, gpointer user_data) {
    // crear ventana
    window = gtk_application_window_new(app);
    // establecer título inicial
    char title[256];
    snprintf(title, sizeof(title), "retchat - %s@%s - %s:%d",
             current_nick, current_room, server_ip_str, server_port);
    gtk_window_set_title(GTK_WINDOW(window), title);
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 500);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // área de mensajes
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);

    // field de texto
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "escribe un mensaje...");
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    g_signal_connect(entry, "activate", G_CALLBACK(on_send_clicked), NULL);

    GtkWidget *send_btn = gtk_button_new_with_label("enviar");
    gtk_box_pack_start(GTK_BOX(hbox), send_btn, FALSE, FALSE, 0);
    g_signal_connect(send_btn, "clicked", G_CALLBACK(on_send_clicked), NULL);

    gtk_widget_show_all(window);
}

int main(int argc, char *argv[]) {
    // procesar argumentos (IP y puerto)
    const char *server_ip = (argc > 1) ? argv[1] : LOCALHOST;
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;

    // guardar IP y puerto en globales
    strncpy(server_ip_str, server_ip, sizeof(server_ip_str) - 1);
    server_ip_str[sizeof(server_ip_str) - 1] = '\0';
    server_port = port;

    /* RED */
    struct sockaddr_in server_addr;

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

    if (recv(sockfd, &server_pub, sizeof(server_pub), 0) != sizeof(server_pub)) {
        fprintf(stderr, "error al recibir clave pública del server\n");
        close(sockfd);
        exit(1);
    }

    if (send(sockfd, &client_pub, sizeof(client_pub), 0) != sizeof(client_pub)) {
        fprintf(stderr, "error enviando clave pública\n");
        close(sockfd);
        exit(1);
    }

    uint64_t shared_secret = compute_shared_secret(server_pub, client_priv);
    kdf_derive(shared_secret, enc_key, KEY_LENGTH);

    /* GTK */
    GtkApplication *app = gtk_application_new("me.retucio.retchat.gtk", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    // crear hilo receptor (debe empezar después de que la UI esté activa)
    // pero la conexión ya está hecha, lanzamos el hilo antes de ejecutar el loop
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        perror("pthread_create");
        close(sockfd);
        exit(1);
    }

    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);

    // si salimos del loop, cerrar socket y el hilo
    close(sockfd);
    pthread_cancel(recv_thread);

    return status;
}
