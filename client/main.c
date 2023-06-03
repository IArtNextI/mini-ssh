#include "common.h"
#include "network.h"
#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
            snprintf(payload + SCMDLEN + SCMDLENLEN + written, length_of_message - SCMDLEN - SCMDLENLEN - written, "%s ", argv[i]);
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

        free(payload);
    }
    else {
        printf("Unknown command : %s", argv[2]);
    }

    close(sock_fd);

    return 0;
}
