#pragma once

#include "common.h"

bool acquire_tcp_listening_socket(const char* port, int* sock_fd, char* error_message, size_t max_error_message_len);

bool acquire_tcp_client_socket(const char* host, const char* port, int* sock_fd, char* error_message, size_t max_error_message_len);

bool read_n_bytes_from_socket_fd(int fd, void* buffer, size_t bytes, char* error_message, size_t max_error_message_len);

bool write_n_bytes_to_socket_fd(int fd, void* buffer, size_t bytes, char* error_message, size_t max_error_message_len);
