#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_EVENTS 1024
#define BUFFER_SIZE 4096
#define FILE_CHUNK 8192

typedef struct {
    int fd;
    char filename[256];
    FILE* file;
    long file_size;
    long total_sent;
    int header_sent;
    int is_directory;
    DIR* dir;
} client_state;

int server_fd, epoll_fd;
client_state* clients[MAX_EVENTS];

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void log_message(const char* type, const char* message, int client_fd) {
    time_t now;
    time(&now);
    struct tm* local = localtime(&now);

    printf("[%02d:%02d:%02d] [%s] ",
           local->tm_hour, local->tm_min, local->tm_sec, type);

    if (client_fd >= 0) {
        printf("Client %d: ", client_fd);
    }
    printf("%s\n", message);
    fflush(stdout);
}

void remove_client(int fd) {
    if (fd < 0 || fd >= MAX_EVENTS) return;

    if (clients[fd]) {
        log_message("INFO", "Client disconnected", fd);
        if (clients[fd]->file) {
            fclose(clients[fd]->file);
        }
        if (clients[fd]->dir) {
            closedir(clients[fd]->dir);
        }
        free(clients[fd]);
        clients[fd] = NULL;
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

void parse_request(char* buffer, char* filename, int* is_directory_request) {
    char method[16] = {0};
    char path[256] = {0};
    char version[32] = {0};

    sscanf(buffer, "%15s %255s %31s", method, path, version);

    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) {
        *is_directory_request = 1;
        filename[0] = '\0';
        return;
    }

    *is_directory_request = 0;

    if (path[0] == '/') {
    strncpy(filename, path + 1, 254);
} else {
    strncpy(filename, path, 254);
}
filename[254] = '\0';

    char* question = strchr(filename, '?');
    if (question) *question = '\0';
}

void handle_directory_listing(int fd) {
    DIR* dir = opendir("./files");
    if (!dir) {
        char* msg = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nCannot read directory";
        send(fd, msg, strlen(msg), MSG_NOSIGNAL);
        log_message("ERROR", "Cannot open files directory", fd);
        remove_client(fd);
        return;
    }

    char* header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
    send(fd, header, strlen(header), MSG_NOSIGNAL);

    struct dirent* entry;
    char line[512];
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char filepath[512];
        snprintf(filepath, sizeof(filepath), "./files/%s", entry->d_name);
        struct stat st;
        long size = 0;
        if (stat(filepath, &st) == 0) {
            size = st.st_size;
        }

        snprintf(line, sizeof(line), "%s %ld\n", entry->d_name, size);
        send(fd, line, strlen(line), MSG_NOSIGNAL);
        count++;
    }

    closedir(dir);
    log_message("INFO", "Directory listing sent", fd);
    remove_client(fd);
}

void handle_file_write(int fd) {
    client_state* state = clients[fd];
    if (!state) return;

    if (!state->header_sent) {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "./files/%s", state->filename);

        struct stat st;
        if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
            char* msg = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found!";
            send(fd, msg, strlen(msg), MSG_NOSIGNAL);
            log_message("ERROR", "File not found", fd);
            remove_client(fd);
            return;
        }

        state->file = fopen(filepath, "rb");
        if (!state->file) {
            char* msg = "HTTP/1.1 500 Internal Error\r\n\r\n";
            send(fd, msg, strlen(msg), MSG_NOSIGNAL);
            remove_client(fd);
            return;
        }

        state->file_size = st.st_size;
        char header[256];
        snprintf(header, sizeof(header), 
                 "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\nContent-Type: application/octet-stream\r\n\r\n", 
                 state->file_size);
        
        if (send(fd, header, strlen(header), MSG_NOSIGNAL) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            remove_client(fd);
            return;
        }
        state->header_sent = 1;
    }

    // Stream file contents
    char buffer[FILE_CHUNK];
    while (state->total_sent < state->file_size) {
        size_t to_read = sizeof(buffer);
        if (state->file_size - state->total_sent < to_read) {
            to_read = state->file_size - state->total_sent;
        }

        long current_pos = ftell(state->file);
        size_t bytes_read = fread(buffer, 1, to_read, state->file);
        if (bytes_read <= 0) break;

        ssize_t sent = send(fd, buffer, bytes_read, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Seek back to re-read unsent portion on next EPOLLOUT event
                fseek(state->file, current_pos, SEEK_SET);
                return;
            }
            remove_client(fd);
            return;
        }

        state->total_sent += sent;
        if (sent < bytes_read) {
            // Seek back for unsent remaining bytes in current chunk
            fseek(state->file, current_pos + sent, SEEK_SET);
            return;
        }
    }

    log_message("COMPLETE", "File transfer complete", fd);
    remove_client(fd);
}

void setup_server() {
    signal(SIGPIPE, SIG_IGN);

    mkdir("./files", 0755);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(server_fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }

    if (listen(server_fd, MAX_EVENTS) < 0) {
        perror("listen"); exit(1);
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) { perror("epoll_create"); exit(1); }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    printf("========================================\n");
    printf("TCP FILE SERVER STARTED (Non-blocking Reactor Mode)\n");
    printf("Port: %d\n", PORT);
    printf("Files Directory: ./files/\n");
    printf("========================================\n\n");
}

int main() {
    setup_server();
    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                struct sockaddr_in addr;
                socklen_t addr_len = sizeof(addr);
                int client = accept(server_fd, (struct sockaddr*)&addr, &addr_len);
                if (client < 0) continue;

                if (client >= MAX_EVENTS) {
                    close(client);
                    continue;
                }

                set_nonblocking(client);

                client_state* state = calloc(1, sizeof(client_state));
                state->fd = client;
                clients[client] = state;

                struct epoll_event ev;
                ev.events = EPOLLIN;
                ev.data.fd = client;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client, &ev);

                log_message("CONNECT", "New client connected", client);
            } 
            else if (events[i].events & EPOLLIN) {
                char buffer[BUFFER_SIZE];
                ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

                if (bytes <= 0) {
                    remove_client(fd);
                    continue;
                }

                buffer[bytes] = '\0';
                client_state* state = clients[fd];
                if (!state) continue;

                parse_request(buffer, state->filename, &state->is_directory);

                if (state->is_directory) {
                    handle_directory_listing(fd);
                } else {
                    // Switch socket interest to EPOLLOUT to begin file streaming
                    struct epoll_event ev;
                    ev.events = EPOLLOUT;
                    ev.data.fd = fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
                }
            } 
            else if (events[i].events & EPOLLOUT) {
                handle_file_write(fd);
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}
