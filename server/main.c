#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "network.h"
#include "proto.h"

bool launch_spawn_cmd(int read_fd, int write_fd, char* payload, int64_t payload_length, pid_t* child_pid, char* error_message, size_t max_error_message_len) {
    errno = 0;
    pid_t pid = fork();

    if (pid == -1) {
        snprintf(error_message, max_error_message_len, "Failed to fork : %s\n", strerror(errno));
        return false;
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
            fflush(stdout);
            _exit(1);
        }
        const char* cmd = &payload[0];
        char** argvv = calloc(argcc + 1, sizeof(*argvv));
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

        // TODO : also stderr
        if (dup2(read_fd, STDIN_FILENO) < 0) {
            printf("Falied to reopen the usr fd as STDIN\n");
            fflush(stdout);
            _exit(1);
        }
        if (dup2(write_fd, STDOUT_FILENO) < 0) {
            printf("Falied to reopen the usr fd as STDOUT\n");
            fflush(stdout);
            _exit(1);
        }

        if (execvp(cmd, argvv) < 0) {
            _exit(1);
        }
    }
    else {
        *child_pid = pid;
    }

    return true;
}

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
        usr = accept(sockfd, NULL, NULL);
        printf("Exited accept...\n");
        fflush(stdout);
        if (errno != 0) {
            // dunno, smth went wrong. retry
            continue;
        }
        printf("Accepted a conection from user. fd : %d\n", usr);
        fflush(stdout);
        // interaction protocol is the following:
        // [main cmd] - 4 bytes. Symbolic. First byte also indicates thansmission mode for length
        // s = symbolic
        // b = binary
        // [length of payload + args block] - 8 bytes. network endianness
        // [payload + args] - variable length.

        // here, I need to lauch a proxy process
        pid_t proxy_pid = fork();

        if (proxy_pid == -1) {
            printf("Failed to launch proxy process\n");
            fflush(stdout);
        }
        else if (proxy_pid == 0) {
            // errno = 0;
            // int usr2 = dup(usr);
            // if (usr2 < -1) {
            //     printf("Failed to duplicate an fd : %s\n", strerror(errno));
            // }
            int pipe_fds[2];
            errno = 0;
            if (pipe(pipe_fds)) {
                printf("Failed to create pipe : %s\n", strerror(errno));
                _exit(1);
            }
            message msg;
            if (!retrieve_message_from_socket(usr, &msg, error_message_buffer, sizeof(error_message_buffer))) {
                printf("Failed to retrieve message from socket : %s\n", error_message_buffer);
                _exit(1);
            }
            // now I need to clear msg.payload whenever I finish the work
            if (!strcmp("spwn", msg.cmd)) {
                printf("recognized a spawn payload!\n");
                fflush(stdout);
                pid_t child_pid;
                if (!launch_spawn_cmd(pipe_fds[0], usr, msg.payload, msg.payload_length, &child_pid, error_message_buffer, sizeof(error_message_buffer))) {
                    printf("Failed to lauch a spawn command\n");
                    _exit(1);
                }
                while (true) {
                    char buffer[4096];
                    while (true) {
                        errno = 0;
                        int rs = read(usr, &buffer, sizeof(buffer));
                        if (rs <= 0 && errno != EINTR) {
                            break;
                        }
                        if (rs > 0) {
                            if (!write_n_bytes_to_socket_fd(pipe_fds[1], buffer, rs, error_message_buffer, sizeof(error_message_buffer))) {
                                printf("Failed to pass server out to STDOUT\n");
                                _exit(1);
                            }
                        }
                    }
                }
                printf("Exited reader loop\n");
            }
            else {
                printf("Unknown cmd : %s\n", msg.cmd);
            }
            fflush(stdout);
            free(msg.payload);
            _exit(0);
        }
        else {
            // main process
            // vibin.
        }
        close(usr);
    }

    close(sockfd);
    return 0;
}