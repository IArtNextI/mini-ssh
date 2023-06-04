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
            // [length of payload + args block] - 8 bytes. network endianness
            // [payload + args] - variable length.
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
                // it's a spawn payload!
                printf("recognized a spawn payload!\n");
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
                char* payload = calloc(payload_length, 1);
                if (!read_n_bytes_from_socket_fd(usr, payload, payload_length, error_message_buffer, sizeof(error_message_buffer))) {
                    close(usr);
                    usr = -1;
                    printf("Failed to retrieve payload size : %s\n", error_message_buffer);
                    fflush(stdout);
                    break;
                }
                printf("Received the payload:\n");
                for (int64_t i = 0; i < payload_length; ++i) {
                    printf("%d %c\n", payload[i], payload[i]);
                }
                fflush(stdout);

                errno = 0;
                pid_t pid = fork();

                if (pid == -1) {
                    printf("Failed to fork : %s\n", strerror(errno));
                }
                else if (pid == 0) {
                    // child
                    int argcc = 0;
                    for (int i = 0; i < payload_length; ++i) {
                        if (payload[i] == '\0') {
                            ++argcc;
                        }
                    }
                    if (argcc == 0) {
                        printf("Malformed payload : no cmd\n");
                        close(usr);
                        usr = -1;
                        fflush(stdout);
                        _exit(1);
                    }
                    const char* cmd = &payload[0];
                    char** argvv = calloc(argcc + 1, sizeof(*argv));
                    int cnt = 0;
                    printf("Forming the args for subprocess...\n");
                    fflush(stdout);
                    for (int64_t i = 0; i < payload_length; ++i) {
                        if (i == 0 || i > 0 && payload[i - 1] == '\0') {
                            // TODO : once again I met the separator.
                            // The correct way to handle it is to pass the number of args + lengths in the message
                            argvv[cnt++] = &payload[i];
                            printf("%s\n", argvv[cnt - 1]);
                        }
                    }

                    int usr2;
                    if ((usr2 = dup(usr)) < 0) {
                        printf("Falied to duplicate the usr fd\n");
                        fflush(stdout);
                        _exit(1);
                    }

                    // TODO : also stderr
                    if (dup2(usr, STDIN_FILENO) < 0) {
                        printf("Falied to reopen the usr fd as STDIN\n");
                        fflush(stdout);
                        _exit(1);
                    }
                    if (dup2(usr2, STDOUT_FILENO) < 0) {
                        printf("Falied to reopen the usr fd as STDOUT\n");
                        fflush(stdout);
                        _exit(1);
                    }



                    if (execvp(cmd, argvv) < 0) {
                        _exit(1);
                    }
                }
                else {
                    // parent
                    int status;
                    if (waitpid(pid, &status, -1) != pid) {
                        printf("Failed to await the process with pid = %d\n", pid);
                        close(usr);
                        usr = -1;
                        break;
                    }
                }

                free(payload);
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