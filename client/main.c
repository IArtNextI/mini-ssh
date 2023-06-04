#include "common.h"
#include "network.h"
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
        int64_t length_of_message = SCMDLEN + SCMDLENLEN + length_of_payload;
        char* payload = calloc(length_of_message, 1);

        snprintf(payload, length_of_message, SCMDSPAWN);

        int64_t length_of_payload_be64 = htobe64(length_of_payload);

        memcpy(payload + SCMDLEN, &length_of_payload_be64, sizeof(length_of_payload_be64));

        int64_t written = 0;
        for (int i = 3; i < argc; ++i) {
            snprintf(payload + SCMDLEN + SCMDLENLEN + written, length_of_message - SCMDLEN - SCMDLENLEN - written, "%s", argv[i]);
            written += strlen(argv[i]) + 1;
        }
        payload[length_of_message - 1] = '\0';

        printf("%ld %ld\n", length_of_message, length_of_payload);
        for (int i = 0; i < length_of_message; ++i) {
            printf("%d %c\n", payload[i], payload[i]);
        }

        if (!write_n_bytes_to_socket_fd(sock_fd, payload, length_of_message, error_message_buffer, sizeof(error_message_buffer))) {
            printf("Failed to send the message to server : %s", error_message_buffer);
        }
        else {
            printf("Successfully transferred the payload to server\n");
        }
        // now I need to reopen fds for STDIN and STDOUT
        // TODO : STDERR also
        // TODO : move this up? pretty far into a process to do things independently of network
        //        interaction logic

        // int sock_fd2;
        // errno = 0;
        // if ((sock_fd2 = dup(sock_fd)) < 0) {
        //     printf("Failed to duplicate socket fd : %s\n", strerror(errno));
        //     return 1;
        // }

        // plan is the following:

        // errno = 0;
        // if (dup2(sock_fd, STDIN_FILENO) < 0) {
        //     printf("Failed to reopen socket fd as STDIN : %s\n", strerror(errno));
        //     return 1;
        // }

        // errno = 0;
        // if (dup2(sock_fd2, STDOUT_FILENO) < 0) {
        //     printf("Failed to reopen socket fd as STDOUT : %s\n", strerror(errno));
        //     return 1;
        // }

        // if (fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
        //     printf("Failed to set socket fd nonblocking\n");
        //     return 1;
        // }

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

        free(payload);
    }
    else {
        printf("Unknown command : %s", argv[2]);
    }

    close(sock_fd);

    return 0;
}
