#include "network.h"
#include <stdio.h>

bool acquire_tcp_listening_socket(const char* port, int* sock_fd, char* error_message, size_t max_error_message_len) {
    struct addrinfo* ai;
    struct addrinfo hint = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    int sfd;
    int ret = getaddrinfo(NULL, port, &hint, &ai);
    if (ret != 0) {
        snprintf(error_message, max_error_message_len, "%s", gai_strerror(ret));
        return false;
    }
    else {
        struct addrinfo* cai = ai;
        while (cai != NULL) {
            sfd = socket(cai->ai_family, cai->ai_socktype, IPPROTO_TCP);
            if (sfd < 0) {
                sfd = -1;
                cai = cai->ai_next;
                continue;
            }
            int one = 1;
            if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
                close(sfd);
                sfd = -1;
                cai = cai->ai_next;
                continue;
            }
            if (bind(sfd, cai->ai_addr, cai->ai_addrlen) < 0) {
                close(sfd);
                sfd = -1;
                cai = cai->ai_next;
                continue;
            }
            if (listen(sfd, SOMAXCONN) < 0) {
                close(sfd);
                sfd = -1;
                cai = cai->ai_next;
                continue;
            }
            break;
        }
        freeaddrinfo(ai);
    }
    if (sfd == -1) {
        snprintf(error_message, max_error_message_len, "All known addresses failed during resolution");
        return false;
    }
    *sock_fd = sfd;
    return true;
}

bool acquire_tcp_client_socket(const char* host, const char* port, int* sock_fd, char* error_message, size_t max_error_message_len) {
    int sfd = -1;
    struct addrinfo* ai;
    struct addrinfo hint = {
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    int ret = getaddrinfo(host, port, &hint, &ai);
    if (ret != 0) {
        snprintf(error_message, max_error_message_len, "%s", gai_strerror(ret));
        return false;
    }
    else {
        struct addrinfo* cai = ai;
        while (cai != NULL) {
            sfd = socket(cai->ai_family, cai->ai_socktype, IPPROTO_TCP);
            if (sfd < 0) {
                cai = cai->ai_next;
                continue;
            }
            if (connect(sfd, cai->ai_addr, cai->ai_addrlen) < 0) {
                close(sfd);
                sfd = -1;
                cai = cai->ai_next;
                continue;
            }
            break;
        }
        freeaddrinfo(ai);
    }
    if (sfd == -1) {
        snprintf(error_message, max_error_message_len, "Failed to resolve provided address");
        return false;
    }
    *sock_fd = sfd;
    return true;
}

bool read_n_bytes_from_socket_fd(int fd, void* buffer, size_t bytes, char* error_message, size_t max_error_message_len) {
    size_t total_read = 0;
    while (total_read < bytes) {
        errno = 0;
        int64_t cur = read(fd, buffer + total_read, bytes - total_read);
        if (cur == 0) {
            snprintf(error_message, max_error_message_len, "Peer disconnected.");
            memset(buffer, 0, bytes);
            return false;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno != 0) {
            snprintf(error_message, max_error_message_len, "Encountered an error while reading from socket : %s", strerror(errno));
            return false;
        }
        total_read += cur;
    }
    return true;
}

bool write_n_bytes_to_socket_fd(int fd, void* buffer, size_t bytes, char* error_message, size_t max_error_message_len) {
    int64_t written = 0;
    while (written < bytes) {
        errno = 0;
        int64_t cur = write(fd, buffer + written, bytes - written);
        if (cur == 0) {
            snprintf(error_message, max_error_message_len, "Unexpected EOF");
            return false;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno != 0) {
            snprintf(error_message, max_error_message_len, "Encountered an error while writing to socket : %s", strerror(errno));
            return false;
        }
        written += cur;
    }
    return true;
}

