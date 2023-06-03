#include <stdio.h>
#include <string.h>

#include "common.h"
#include "network.h"

const int SCMDLEN = 4;

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage : %s PORT\n", argv[0]);
        FLUSH_STDOUT_EXIT(1);
    }
    printf("Starting daemon on port %s...\n", argv[1]);
    printf("Acquiring socket...\n");
    fflush(stdout);
    int sockfd;
    char buffer[128];
    if (!acquire_tcp_listening_socket(argv[1], &sockfd, buffer, sizeof(buffer))) {
        printf("Failed to acquire socket : %s\n", buffer);
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
        bool finished = false;
        while (!finished) {
            // interaction protocol is the following:
            // [main cmd] - 4 bytes. Symbolic. First byte also indicates thansmission mode for length
            // s = symbolic
            // b = binary
            // [length of command + args block] - 8 bytes. network endianness
            // [command + args] - variable length.
            char server_cmd[SCMDLEN + 1];
            int total_read = 0;
            while (total_read < SCMDLEN) {
                errno = 0;
                int64_t cur = read(usr, server_cmd + total_read, SCMDLEN - total_read);
                if (cur == 0) {
                    printf("User disconnected. Closing fd : %d...\n", usr);
                    fflush(stdout);
                    close(usr);
                    usr = -1;
                    // disconnect;
                    finished = true;
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno != 0) {
                    printf("Encountered an error while reading from socket : %s", strerror(errno));
                    memset(server_cmd, 0, sizeof(server_cmd));
                    break;
                }
                total_read += cur;
            }
            if (finished) continue;
            server_cmd[SCMDLEN] = '\0';
            if (!strcmp("spwn", server_cmd)) {
                // it's a spawn command!
                printf("recognized a spawn command!\n");
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