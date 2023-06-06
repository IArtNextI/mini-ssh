#include "common.h"
#include "network.h"
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "proto.h"

void* run(void* arg) {
    int read_fd = *(int*)arg;
    char buffer[4096];
    char error_message_buffer[128];
    while (true) {
        int rs = read(read_fd, &buffer, sizeof(buffer));
        if (rs > 0) {
            if (!write_n_bytes_to_socket_fd(STDOUT_FILENO, buffer, rs, error_message_buffer, sizeof(error_message_buffer))) {
                printf("Failed to pass server out to STDOUT\n");
                exit(1);
            }
        }
        if (rs == 0) {
            break;
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage : %s <server-address> <cmd>", argv[0]);
        FLUSH_STDOUT_EXIT(1);
    }
    // argv[1] is of format ip:port
    const char* ip = argv[1];
    const char* port = "-1";
    for (int i = 0; i < strlen(argv[1]); ++i) {
        if (argv[1][i] == ':') {
            argv[1][i] = '\0';
            port = &argv[1][i + 1];
        }
    }

    int sock_fd;
    char error_message_buffer[128];
    if (!acquire_tcp_client_socket(ip, port, &sock_fd, error_message_buffer, sizeof(error_message_buffer))) {
        printf("Failed to acquire client-side socket : %s", error_message_buffer);
        FLUSH_STDOUT_EXIT(1);
    }

    if (!strcmp(argv[2], "spawn")) {
        printf("Client received spawn command\n");
        int64_t length_of_payload = 0;
        for (int i = 3; i < argc; ++i) {
            length_of_payload += strlen(argv[i]) + 1;
        }

        message msg;
        msg.cmd[0] = 's';
        msg.cmd[1] = 'p';
        msg.cmd[2] = 'w';
        msg.cmd[3] = 'n';
        msg.payload_length = length_of_payload;
        msg.payload = calloc(length_of_payload, 1);

        int64_t written = 0;
        for (int i = 3; i < argc; ++i) {
            snprintf(msg.payload + written, length_of_payload - SCMDLEN - sizeof(int64_t) - written, "%s", argv[i]);
            written += strlen(argv[i]) + 1;
        }
        msg.payload[length_of_payload - 1] = '\0';

        for (int i = 0; i < msg.payload_length; ++i) {
            printf("%d %c\n", msg.payload[i], msg.payload[i]);
        }

        if (!send_message_through_socket(sock_fd, &msg, error_message_buffer, sizeof(error_message_buffer))) {
            printf("Failed to send message to server : %s", error_message_buffer);
            FLUSH_STDOUT_EXIT(1);
        }

        pthread_t server_reader_thread;

        if (pthread_create(&server_reader_thread, NULL, run, &sock_fd) != 0) {
            printf(("Failed to create server reader thread.\n"));
            return 1;
        }

        char buffer[4096];
        while (true) {
            int r = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (r > 0) {
                if (!write_n_bytes_to_socket_fd(sock_fd, buffer, r, error_message_buffer, sizeof(error_message_buffer))) {
                    printf("Failed to pass the read byte : %s\n", error_message_buffer);
                }
            }
            if (r == 0) {
                break;
            }
        }
        printf("Exited from the reader loop\n");
        errno = 0;
        if (shutdown(sock_fd, SHUT_RDWR)) {
            printf("Failed to shutdown the socket : %s\n", strerror(errno));
        }
    }
    else {
        printf("Unknown command : %s", argv[2]);
    }

    close(sock_fd);

    return 0;
}
