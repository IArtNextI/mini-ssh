#pragma once

#include <arpa/inet.h>
#include <bits/types/sigset_t.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define FLUSH_STDOUT_EXIT(ret_code) \
    fflush(stdout);                 \
    exit(ret_code);

#define SCMDLEN 4
#define SCMDLENLEN 8
#define SCMDSPAWN "spwn"
