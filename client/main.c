#include "common.h"
#include "network.h"

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
    char buffer[128];
    if (!acquire_tcp_client_socket(ip, port, &sock_fd, buffer, sizeof(buffer))) {
        printf("Failed to acquire client-side socket : %s", buffer);
        FLUSH_STDOUT_EXIT(1);
    }

    if (!strcmp(argv[2], "spawn")) {
        printf("Client received spawn command\n");
    }
    else {
        printf("Unknown command : %s", argv[2]);
    }

    return 0;
}
