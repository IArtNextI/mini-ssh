#pragma once

#include "common.h"

bool acquire_tcp_listening_socket(const char* port, int* sock_fd, char* error_message, size_t max_error_message_len);

bool acquire_tcp_client_socket(const char* host, const char* port, int* sock_fd, char* error_message, size_t max_error_message_len);
