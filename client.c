#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#include <ctype.h>
#include <dirent.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

void clear_screen() {
    printf("\033[2J\033[H");
}

void print_header() {
    clear_screen();
    printf(COLOR_BOLD COLOR_CYAN);
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║              TCP FILE SERVER - CLIENT MENU               ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf(COLOR_RESET);
    printf(COLOR_CYAN "\n📡 Connected to: 127.0.0.1:%d\n" COLOR_RESET, PORT);
    char path[512];
    if (getcwd(path, sizeof(path))) {
        printf("📂 Current directory: %s\n", path);
    }
    printf("\n");
}

void print_menu() {
    printf(COLOR_YELLOW "📋 MAIN MENU\n" COLOR_RESET);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  %s1%s. 📂 Check available files on server\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s2%s. ⬇️  Download a file\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s3%s. 📜 Check download history\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s4%s. 📁 Check downloaded files\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s5%s. 🚪 Exit\n", COLOR_GREEN, COLOR_RESET);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
}

void list_server_files() {
    printf(COLOR_YELLOW "\n📂 Fetching available files from server...\n" COLOR_RESET);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf(COLOR_RED "❌ Failed to create socket\n" COLOR_RESET);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf(COLOR_RED "❌ Could not connect to server. Is the server running?\n" COLOR_RESET);
        close(sock);
        return;
    }

    char request[] = "GET / HTTP/1.1\r\n\r\n";
    send(sock, request, strlen(request), 0);

    // Dynamic buffer to accumulate full HTTP response across multiple recv() calls
    char full_response[8192] = {0};
    int total_bytes = 0;
    int bytes = 0;

    // Read full stream until connection closes or buffer fills
    while ((bytes = recv(sock, full_response + total_bytes, sizeof(full_response) - total_bytes - 1, 0)) > 0) {
        total_bytes += bytes;
    }
    full_response[total_bytes] = '\0';

    if (total_bytes > 0) {
        // Locate end of HTTP headers
        char* body = strstr(full_response, "\r\n\r\n");
        if (body) {
            body += 4; // Move past \r\n\r\n delimiter
            
            if (strlen(body) > 0) {
                printf(COLOR_GREEN "✅ Connected to server!\n" COLOR_RESET);
                printf("\n📋 Available files (Name / Size in bytes):\n");
                printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
                printf("%s", body);
                printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
            } else {
                printf(COLOR_YELLOW "📭 No files found in server directory.\n" COLOR_RESET);
            }
        } else {
            printf(COLOR_RED "❌ Malformed HTTP response header received from server.\n" COLOR_RESET);
        }
    } else {
        printf(COLOR_RED "❌ Failed to receive directory listing from server.\n" COLOR_RESET);
    }

    close(sock);
    printf("\nPress Enter to continue...");
    getchar();
}

void download_file(const char* filename) {
    printf(COLOR_YELLOW "\n⬇️  Downloading: %s\n" COLOR_RESET, filename);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf(COLOR_RED "❌ Could not connect to server. Is it running?\n" COLOR_RESET);
        close(sock);
        return;
    }

    char request[512];
    snprintf(request, sizeof(request), "GET /%s HTTP/1.1\r\n\r\n", filename);
    send(sock, request, strlen(request), 0);

    char buffer[BUFFER_SIZE];
    int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
    if (bytes <= 0) {
        printf(COLOR_RED "❌ Empty response from server\n" COLOR_RESET);
        close(sock);
        return;
    }

    // Check HTTP status
    if (strncmp(buffer, "HTTP/1.1 200 OK", 15) != 0) {
        printf(COLOR_RED "❌ File not found on server!\n" COLOR_RESET);
        close(sock);
        return;
    }

    // Find end of HTTP headers
    char* body = strstr(buffer, "\r\n\r\n");
    if (!body) {
        printf(COLOR_RED "❌ Malformed HTTP response\n" COLOR_RESET);
        close(sock);
        return;
    }
    body += 4;

    FILE* output = fopen(filename, "wb");
    if (!output) {
        perror("fopen");
        close(sock);
        return;
    }

    int header_offset = body - buffer;
    int payload_in_first_chunk = bytes - header_offset;

    if (payload_in_first_chunk > 0) {
        fwrite(body, 1, payload_in_first_chunk, output);
    }

    int total_bytes = payload_in_first_chunk;

    while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes, output);
        total_bytes += bytes;
    }

    fclose(output);
    close(sock);

    if (total_bytes > 0) {
        printf(COLOR_GREEN "✅ Downloaded: %s (%d bytes / %.2f KB)\n" COLOR_RESET,
               filename, total_bytes, total_bytes / 1024.0);

        FILE* history = fopen("download_history.txt", "a");
        if (history) {
            time_t now = time(NULL);
            struct tm* tm = localtime(&now);
            fprintf(history, "[%02d:%02d:%02d] %s - %d bytes\n",
                    tm->tm_hour, tm->tm_min, tm->tm_sec, filename, total_bytes);
            fclose(history);
        }
    } else {
        printf(COLOR_RED "❌ Download failed!\n" COLOR_RESET);
        remove(filename);
    }
}

void download_file_menu() {
    char filename[256];
    printf(COLOR_YELLOW "\n📝 Enter filename to download: " COLOR_RESET);
    if (scanf("%255s", filename) == 1) {
        getchar();
        download_file(filename);
    }
    printf("\nPress Enter to continue...");
    getchar();
}

void view_history() {
    printf(COLOR_YELLOW "\n📜 DOWNLOAD HISTORY\n" COLOR_RESET);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    FILE* history = fopen("download_history.txt", "r");
    if (!history) {
        printf(COLOR_YELLOW "📭 No download history found.\n" COLOR_RESET);
    } else {
        char line[256];
        int count = 1;
        printf(COLOR_CYAN "  #  Time        File                 Size\n" COLOR_RESET);
        printf("  ────────────────────────────────────────────\n");
        while (fgets(line, sizeof(line), history)) {
            line[strcspn(line, "\n")] = 0;
            printf("  %2d. %s\n", count++, line);
        }
        fclose(history);
        printf("  ────────────────────────────────────────────\n");
        printf(COLOR_CYAN "  Total: %d downloads\n" COLOR_RESET, count - 1);
    }
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("\nPress Enter to continue...");
    getchar();
}

void list_downloaded_files() {
    printf(COLOR_YELLOW "\n📁 DOWNLOADED FILES\n" COLOR_RESET);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    DIR* dir = opendir(".");
    if (!dir) {
        printf(COLOR_RED "❌ Failed to open directory\n" COLOR_RESET);
        return;
    }

    struct dirent* entry;
    int count = 0;
    long total_size = 0;

    printf(COLOR_CYAN "  📄 File Name               Size (bytes)\n" COLOR_RESET);
    printf("  ────────────────────────────────────────────\n");

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        struct stat st;
        if (stat(entry->d_name, &st) == 0 && S_ISREG(st.st_mode)) {
            if (strcmp(entry->d_name, "server") == 0 ||
                strcmp(entry->d_name, "client") == 0 ||
                strcmp(entry->d_name, "server.c") == 0 ||
                strcmp(entry->d_name, "client.c") == 0 ||
                strcmp(entry->d_name, "common.h") == 0) {
                continue;
            }
            printf("  📄 %-20s %10ld bytes\n", entry->d_name, st.st_size);
            count++;
            total_size += st.st_size;
        }
    }

    closedir(dir);

    if (count == 0) {
        printf(COLOR_YELLOW "  📭 No downloaded files found.\n" COLOR_RESET);
    } else {
        printf("  ────────────────────────────────────────────\n");
        printf(COLOR_CYAN "  Total: %d files, %ld bytes (%.2f KB)\n" COLOR_RESET,
               count, total_size, total_size / 1024.0);
    }

    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("\nPress Enter to continue...");
    getchar();
}

int main() {
    int choice;

    print_header();
    printf(COLOR_GREEN "Welcome to the TCP File Server Client!\n" COLOR_RESET);
    sleep(1);

    while (1) {
        print_header();
        print_menu();

        printf(COLOR_YELLOW "👉 Enter your choice: " COLOR_RESET);
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }
        getchar();

        switch (choice) {
            case 1: list_server_files(); break;
            case 2: download_file_menu(); break;
            case 3: view_history(); break;
            case 4: list_downloaded_files(); break;
            case 5:
                printf(COLOR_GREEN "\nGoodbye! 👋\n\n" COLOR_RESET);
                exit(0);
            default:
                printf(COLOR_RED "❌ Invalid choice! Select 1-5.\n" COLOR_RESET);
                printf("\nPress Enter to continue...");
                getchar();
        }
    }

    return 0;
}
