#include <asm-generic/errno-base.h>
#include <endian.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "network.h"
#include "proto.h"

sig_atomic_t terminated = 0;

void handle(int x) {
    terminated = 1;
}

bool launch_spawn_cmd(int read_fd, int close_fd, int write_fd, char* payload, int64_t payload_length, pid_t* child_pid, char* error_message, size_t max_error_message_len) {
    errno = 0;
    pid_t pid = fork();

    if (pid == -1) {
        snprintf(error_message, max_error_message_len, "Failed to fork : %s\n", strerror(errno));
        return false;
    }
    else if (pid == 0) {
        close(close_fd);
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
                argvv[cnt++] = &payload[i];
                printf("%s\n", argvv[cnt - 1]);
            }
        }
        if (dup2(read_fd, STDIN_FILENO) < 0) {
            printf("Falied to reopen the usr fd as STDIN\n");
            fflush(stdout);
            _exit(1);
        }
        int err_fd = dup(write_fd);
        if (err_fd < 0) {
            printf("Falied to call 'dup' on write fd\n");
            fflush(stdout);
            _exit(1);
        }
        if (dup2(write_fd, STDOUT_FILENO) < 0) {
            printf("Falied to reopen the usr fd as STDOUT\n");
            fflush(stdout);
            _exit(1);
        }
        if (dup2(err_fd, STDERR_FILENO) < 0) {
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
    struct sigaction sa;
    sa.sa_handler = handle;
    if (sigaction(SIGTERM, &sa, NULL)) {
        printf("Failed to set SIGTERM handler\n");
        FLUSH_STDOUT_EXIT(1);
    }
    // struct sigaction sa_chld;
    // sa.sa_handler = SIG_IGN;
    // if (sigaction(SIGCHLD, &sa, NULL)) {
    //     printf("Failed to set SIGTERM handler\n");
    //     FLUSH_STDOUT_EXIT(1);
    // }
    printf("Starting daemon on port %s...\n", argv[1]);
    if (daemon(1, 0) != 0) {
        printf("Failed to start the daemon\n");
        FLUSH_STDOUT_EXIT(1);
    }
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
    while (!terminated) {
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
                close(pipe_fds[0]);
                close(pipe_fds[1]);
                _exit(1);
            }
            // now I need to clear msg.payload whenever I finish the work
            if (!strncmp("spwn", msg.cmd, SCMDLEN)) {
                printf("recognized a spawn payload!\n");
                fflush(stdout);
                pid_t child_pid;
                if (!launch_spawn_cmd(pipe_fds[0], pipe_fds[1], usr, msg.payload, msg.payload_length, &child_pid, error_message_buffer, sizeof(error_message_buffer))) {
                    printf("Failed to lauch a spawn command\n");
                    close(pipe_fds[0]);
                    close(pipe_fds[1]);
                    _exit(1);
                }
                close(pipe_fds[0]);
                message msg;
                while (true) {
                    if (!retrieve_message_from_socket(usr, &msg, error_message_buffer, sizeof(error_message_buffer))) {
                        printf("Failed to retrieve message from socket : %s\n", error_message_buffer);
                        break;
                    }
                    printf("Received a message with header: %c%c%c%c\n", msg.cmd[0], msg.cmd[1], msg.cmd[2], msg.cmd[3]);
                    if (!strncmp(msg.cmd, "data", SCMDLEN)) {
                        if (!write_n_bytes_to_socket_fd(pipe_fds[1], msg.payload, msg.payload_length, error_message_buffer, sizeof(error_message_buffer))) {
                            printf("Failed to pass server out to STDOUT\n");
                            close(pipe_fds[0]);
                            close(pipe_fds[1]);
                            _exit(1);
                        }
                    }
                    else if (!strncmp(msg.cmd, "sign", SCMDLEN)) {
                        if (!strncmp(msg.payload, "SIGINT", 6)) {
                            // send SIGINT
                            if (kill(child_pid, SIGINT)) {
                                printf("Failed to send SIGINT to child process\n");
                            }
                            close(pipe_fds[0]);
                            close(pipe_fds[1]);
                            break;
                        }
                    }

                    free(msg.payload);
                }
                printf("Exited reader loop\n");
                shutdown(pipe_fds[1], SHUT_WR);
                close(pipe_fds[1]);
                int status;
                int ret_pid;
                // if (kill(child_pid, SIGKILL) != 0) {
                //     printf("Failed to SIGKILL %d\n", ret_pid);
                // }
                if ((ret_pid = waitpid(child_pid, &status, 0)) != child_pid) {
                    printf("Failed to await the runner process with pid = %d. Got %d\n", child_pid, ret_pid);
                }
                else {
                    printf("Successfully terminated the runner process with pid = %d\n", child_pid);
                }
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