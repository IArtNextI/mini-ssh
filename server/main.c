#include <endian.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "network.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage : %s PORT\n", argv[0]);
        FLUSH_STDOUT_EXIT(1);
    }
    printf("Starting daemon on port %s...\n", argv[1]);
    printf("Acquiring socket...\n");
    fflush(stdout);
    int sockfd;
    char error_message_buffer[128];
    if (!acquire_tcp_listening_socket(argv[1], &sockfd, error_message_buffer, sizeof(error_message_buffer))) {
        printf("Failed to acquire socket : %s\n", error_message_buffer);
        FLUSH_STDOUT_EXIT(1);
    }
    printf("Success! Socket fd : %d\n", sockfd);
    fflush(stdout);
    int usr = -1;
    while (true) {
        printf("Awaiting connections...\n");
        fflush(stdout);
        errno = 0;
        struct sockaddr sa;
        socklen_t sal;
        usr = accept(sockfd, &sa, &sal);
        printf("Exited accept...\n");
        fflush(stdout);
        if (errno != 0) {
            // dunno, smth went wrong. retry
            continue;
        }
        printf("Accepted a conection from user. fd : %d\n", usr);
        fflush(stdout);
        // then we have a user. Interact until EOF
        while (true) {
            // interaction protocol is the following:
            // [main cmd] - 4 bytes. Symbolic. First byte also indicates thansmission mode for length
            // s = symbolic
            // b = binary
            // [length of command + args block] - 8 bytes. network endianness
            // [command + args] - variable length.
            char server_cmd[SCMDLEN + 1];
            if (!read_n_bytes_from_socket_fd(usr, server_cmd, SCMDLEN, error_message_buffer, sizeof(error_message_buffer))) {
                close(usr);
                usr = -1;
                printf("Failed to retrieve cmd : %s\n", error_message_buffer);
                fflush(stdout);
                break;
            }
            server_cmd[SCMDLEN] = '\0';
            if (!strcmp("spwn", server_cmd)) {
                // it's a spawn command!
                printf("recognized a spawn command!\n");
                // now I need to receive a big-endian int64_t
                int64_t be64_payload_length;
                if (!read_n_bytes_from_socket_fd(usr, &be64_payload_length, sizeof(be64_payload_length), error_message_buffer, sizeof(error_message_buffer))) {
                    close(usr);
                    usr = -1;
                    printf("Failed to retrieve payload size : %s\n", error_message_buffer);
                    fflush(stdout);
                    break;
                }
                printf("Received a big-endian integer %lx\n", be64_payload_length);
                fflush(stdout);
                int64_t payload_length = be64toh(be64_payload_length);
                printf("Number is %ld\n", payload_length);
                fflush(stdout);
                char* command = calloc(payload_length, 1);
                if (!read_n_bytes_from_socket_fd(usr, command, payload_length, error_message_buffer, sizeof(error_message_buffer))) {
                    close(usr);
                    usr = -1;
                    printf("Failed to retrieve payload size : %s\n", error_message_buffer);
                    fflush(stdout);
                    break;
                }
                printf("Received the payload : '%s'\n", command);
                fflush(stdout);
                free(command);
            }
            else {
                printf("Unknown cmd : %s\n", server_cmd);
            }
            fflush(stdout);
        }
    }

    close(sockfd);
    return 0;
}