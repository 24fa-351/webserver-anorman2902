#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define DEFAULT_PORT 80
#define STATIC_DIR "static"
#define MAX_CONNECTION_QUEUE 10
#define MAX_METHOD_SIZE 16
#define MAX_PROTOCOL_SIZE 16
#define STATIC_PREFIX "/static/"
#define STATIC_PREFIX_LEN 8
#define STATS_ENDPOINT "/stats"
#define STATS_ENDPOINT_LEN 6
#define CALC_ENDPOINT "/calc?"
#define CALC_ENDPOINT_LEN 6
#define PERMISSIONS 0755

static int request_count = 0;
static int received_bytes = 0;
static int sent_bytes = 0;

pthread_mutex_t stats_lock;

void send_response(int client_socket, int status_code, const char *content_type, const char *content, size_t content_length) {
    char header[BUFFER_SIZE];
    int header_length = snprintf(header, BUFFER_SIZE,
        "HTTP/1.1 %d\r\nContent-Type: %s\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
        status_code, content_type, content_length);

    pthread_mutex_lock(&stats_lock);
    sent_bytes += header_length + content_length;
    pthread_mutex_unlock(&stats_lock);

    send(client_socket, header, header_length, 0);
    send(client_socket, content, content_length, 0);
}

void handle_static(int client_socket, const char *path) {
    char filepath[BUFFER_SIZE];
    snprintf(filepath, BUFFER_SIZE, "%s/%s", STATIC_DIR, path + STATIC_PREFIX_LEN);

    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        const char *not_found = "File not found";
        send_response(client_socket, 404, "text/plain", not_found, strlen(not_found));
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        close(file_fd);
        const char *server_error = "Internal server error";
        send_response(client_socket, 500, "text/plain", server_error, strlen(server_error));
        return;
    }

    char *file_content = malloc(file_stat.st_size);
    read(file_fd, file_content, file_stat.st_size);
    close(file_fd);

    send_response(client_socket, 200, "application/octet-stream", file_content, file_stat.st_size);
    free(file_content);
}

void handle_stats(int client_socket) {
    char html[BUFFER_SIZE];

    pthread_mutex_lock(&stats_lock);
    snprintf(html, BUFFER_SIZE,
        "<html><body>"
        "<h1>Server Statistics</h1>"
        "<p>Total requests: %d</p>"
        "<p>Total bytes received: %d</p>"
        "<p>Total bytes sent: %d</p>"
        "</body></html>",
        request_count, received_bytes, sent_bytes);
    pthread_mutex_unlock(&stats_lock);

    send_response(client_socket, 200, "text/html", html, strlen(html));
}

void handle_calc(int client_socket, const char *query) {
    int num1 = 0, num2 = 0;
    if (sscanf(query, "a=%d&b=%d", &num1, &num2) == 2) {
        char result[BUFFER_SIZE];
        int length = snprintf(result, BUFFER_SIZE, "Result: %d", num1 + num2);
        send_response(client_socket, 200, "text/plain", result, length);
    } else {
        const char *bad_request = "Invalid parameters";
        send_response(client_socket, 400, "text/plain", bad_request, strlen(bad_request));
    }
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    int received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (received <= 0) {
        close(client_socket);
        return NULL;
    }

    buffer[received] = '\0';

    pthread_mutex_lock(&stats_lock);
    request_count++;
    received_bytes += received;
    pthread_mutex_unlock(&stats_lock);

    char method[MAX_METHOD_SIZE], path[BUFFER_SIZE], protocol[MAX_PROTOCOL_SIZE];
    sscanf(buffer, "%15s %1023s %15s", method, path, protocol);

    if (strncmp(path, STATIC_PREFIX, STATIC_PREFIX_LEN) == 0) {
        handle_static(client_socket, path);
    } else if (strncmp(path, STATS_ENDPOINT, STATS_ENDPOINT_LEN) == 0) {
        handle_stats(client_socket);
    } else if (strncmp(path, CALC_ENDPOINT, CALC_ENDPOINT_LEN) == 0) {
        handle_calc(client_socket, path + CALC_ENDPOINT_LEN);
    } else {
        const char *not_found = "Not Found";
        send_response(client_socket, 404, "text/plain", not_found, strlen(not_found));
    }

    close(client_socket);
    return NULL;
}

void start_server(int port) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CONNECTION_QUEUE) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

        if (*client_socket < 0) {
            perror("Accept failed");
            free(client_socket);
            continue;
        }

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client_socket);
        pthread_detach(thread);
    }

    close(server_socket);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;

    for (int arg_index = 1; arg_index < argc; arg_index++) {
        if (strcmp(argv[arg_index], "-p") == 0 && arg_index + 1 < argc) {
            char *endptr;
            port = strtol(argv[++arg_index], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "Invalid port number\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    mkdir(STATIC_DIR, PERMISSIONS);
    pthread_mutex_init(&stats_lock, NULL);

    start_server(port);

    pthread_mutex_destroy(&stats_lock);
    return 0;
}
