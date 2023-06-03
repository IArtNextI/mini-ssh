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
