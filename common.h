#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_EVENTS 1024
#define THREAD_COUNT 4
#define BUFFER_SIZE 4096
#define FILE_CHUNK 8192

typedef struct {
    int fd;
    char filename[256];
    int is_reading;
    int bytes_sent;
    int file_size;
    FILE* file;
    char buffer[FILE_CHUNK];
    int buffer_pos;
    int buffer_len;
    pthread_mutex_t lock;
} client_state;

#endif
