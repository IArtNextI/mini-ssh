#include "proto.h"
#include "common.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network.h"

bool retrieve_message_from_socket(int sock_fd, message* result, char* error_message, size_t max_error_message_len) {
    result->payload = NULL;
    if (!read_n_bytes_from_socket_fd(sock_fd, result->cmd, SCMDLEN, error_message, max_error_message_len)) {
        snprintf(error_message, max_error_message_len, "Failed to retrieve cmd : %s\n", error_message);
        return false;
    }
    int64_t be64_payload_length;
    char error_message_buffer[128];
    if (!read_n_bytes_from_socket_fd(sock_fd, &be64_payload_length, sizeof(be64_payload_length), error_message_buffer, sizeof(error_message_buffer))) {
        snprintf(error_message, max_error_message_len, "Failed to retrieve payload size : %s\n", error_message_buffer);
        return false;
    }
    result->payload_length = be64toh(be64_payload_length);
    result->payload = calloc(result->payload_length, 1);
    if (!read_n_bytes_from_socket_fd(sock_fd, result->payload, result->payload_length, error_message_buffer, sizeof(error_message_buffer))) {
        snprintf(error_message, max_error_message_len, "Failed to retrieve payload : %s\n", error_message_buffer);
        free(result->payload);
        result->payload = NULL;
        return false;
    }
    return true;
}

bool send_message_through_socket(int sock_fd, message* msg, char* error_message, size_t max_error_message_len) {
    int64_t length_of_message = SCMDLEN + sizeof(int64_t) + msg->payload_length;
    char* message_buff = calloc(length_of_message, 1);
    int64_t length_of_payload_be64 = htobe64(msg->payload_length);
    memcpy(message_buff, msg->cmd, SCMDLEN);
    memcpy(message_buff + SCMDLEN, &length_of_payload_be64, sizeof(length_of_payload_be64));
    memcpy(message_buff + SCMDLEN + sizeof(int64_t), msg->payload, msg->payload_length);
    return write_n_bytes_to_socket_fd(sock_fd, message_buff, length_of_message, error_message, max_error_message_len);
}

void free_message_internals(message* msg) {
    free(msg->payload);
}
