#include "common.h"
#include "network.h"
#include <bits/types/sigset_t.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
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
        if (rs <= 0) {
            break;
        }
    }
//    printf("Exited from the server-reader loop\n");
    pthread_exit(NULL);
}

sig_atomic_t sigint_in = 0;

void handle(int x) {
    sigint_in = 1;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage : %s <server-address> <cmd>", argv[0]);
        FLUSH_STDOUT_EXIT(1);
    }
    // printf("Launched with %d params\n", argc);
    // for (int i = 0; i < argc; ++i) {
    //     printf("Param %d : %s\n", i, argv[i]);
    // }
    struct sigaction sa;
    sa.sa_handler = handle;
    if (sigaction(SIGINT, &sa, NULL)) {
//        printf("Failed to set SIGINT handler\n");
        FLUSH_STDOUT_EXIT(1);
    }
    sigset_t sigset;
    if (sigemptyset(&sigset)) {
//        printf("Failed to clear sigset\n");
        FLUSH_STDOUT_EXIT(1);
    }
    if (sigaddset(&sigset, SIGINT)) {
//        printf("Failed to add SIGINT to sigset\n");
        FLUSH_STDOUT_EXIT(1);
    }
    if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)) {
//        printf("Failed to add SIGINT to sigset\n");
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
//        printf("Failed to acquire client-side socket : %s", error_message_buffer);
        FLUSH_STDOUT_EXIT(1);
    }

    if (!strcmp(argv[2], "spawn")) {
//        printf("Client received spawn command\n");
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
            snprintf(msg.payload + written, length_of_payload - written, "%s", argv[i]);
            written += strlen(argv[i]) + 1;
        }
        msg.payload[length_of_payload - 1] = '\0';

//        for (int i = 0; i < msg.payload_length; ++i) {
//            printf("%d %c\n", msg.payload[i], msg.payload[i]);
//        }

        if (!send_message_through_socket(sock_fd, &msg, error_message_buffer, sizeof(error_message_buffer))) {
//            printf("Failed to send message to server : %s", error_message_buffer);
            free(msg.payload);
            FLUSH_STDOUT_EXIT(1);
        }
        free(msg.payload);

        pthread_t server_reader_thread;

        if (pthread_create(&server_reader_thread, NULL, run, &sock_fd) != 0) {
//            printf(("Failed to create server reader thread.\n"));
            return 1;
        }

        char buffer[4096];
        char signal_buffer[10];
        while (true) {
            int r = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (sigprocmask(SIG_BLOCK, &sigset, NULL)) {
//                printf("Failed to block SIGINT\n");
                FLUSH_STDOUT_EXIT(1);
            }
            if (r > 0) {
                message msg;
                msg.cmd[0] = 'd';
                msg.cmd[1] = 'a';
                msg.cmd[2] = 't';
                msg.cmd[3] = 'a';
                msg.payload_length = r;
                msg.payload = buffer;
                if (!send_message_through_socket(sock_fd, &msg, error_message_buffer, sizeof(error_message_buffer))) {
//                    printf("Failed to pass the message : %s\n", error_message_buffer);
                }
            }
            if (sigint_in) {
                message msg;
                msg.cmd[0] = 's';
                msg.cmd[1] = 'i';
                msg.cmd[2] = 'g';
                msg.cmd[3] = 'n';
                msg.payload_length = 6;
                signal_buffer[0] = 'S';
                signal_buffer[1] = 'I';
                signal_buffer[2] = 'G';
                signal_buffer[3] = 'I';
                signal_buffer[4] = 'N';
                signal_buffer[5] = 'T';
                msg.payload = signal_buffer;
                if (!send_message_through_socket(sock_fd, &msg, error_message_buffer, sizeof(error_message_buffer))) {
//                    printf("Failed to pass the message : %s\n", error_message_buffer);
                }
                sigint_in = 0;
                break;
            }
            if (r == 0) {
                break;
            }
            if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)) {
//                printf("Failed to unblock SIGINT\n");
                FLUSH_STDOUT_EXIT(1);
            }
        }
//        printf("Exited from the user-reader loop\n");
        errno = 0;
        if (shutdown(sock_fd, SHUT_WR)) {
//            printf("Failed to shutdown the socket : %s\n", strerror(errno));
        }
        if (pthread_join(server_reader_thread, NULL)) {
//            printf("Failed to join thread\n");
        }
    }
    else {
        printf("Unknown command : %s", argv[2]);
    }

    close(sock_fd);

    return 0;
}
