#include "common.h"

typedef struct message {
    char cmd[SCMDLEN + 1];
    int64_t payload_length;
    char* payload;
} message;

bool retrieve_message_from_socket(int sock_fd, message* result, char* error_message, size_t max_error_message_len);
bool send_message_through_socket(int sock_fd, message* msg, char* error_message, size_t max_error_message_len);
void free_message(message* msg);
