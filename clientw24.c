#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>

#define PORT 5151
#define BUFFER 1024
#define DIR_BUFFER 65000
#define SHELL_MESSAGE "client24$ "

// Receive tar file from server
void receive_tar(int sockfd, long file_size, const char *path)
{
    FILE *fp;
    char buffer[BUFFER];
    ssize_t bytes_received;
    long total_bytes_received = 0;

    // Open file to write received tar data
    fp = fopen(path, "wb");
    if (fp == NULL)
    {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }

    // Receive the entire tar file
    while (total_bytes_received < file_size)
    {
        bytes_received = read(sockfd, buffer, BUFFER);
        if (bytes_received < 0)
        {
            perror("Error receiving tar data");
            exit(EXIT_FAILURE);
        }
        total_bytes_received += bytes_received;
        fwrite(buffer, 1, bytes_received, fp);
        // memset(buffer, 0, sizeof(buffer));
    }

    // Close the file
    fclose(fp);

    printf("Received tar file from server.\n");
}

// Receive text file from server
void receive_text(int sockfd, long file_size)
{
    char buffer[BUFFER];
    ssize_t bytes_received;
    long total_bytes_received = 0;

    // Receive the entire tar file
    while (total_bytes_received < file_size)
    {
        bytes_received = read(sockfd, buffer, BUFFER);
        printf("%s", buffer);
        if (bytes_received < 0)
        {
            perror("Error receiving tar data");
            exit(EXIT_FAILURE);
        }
        total_bytes_received += bytes_received;
        memset(buffer, 0, sizeof(buffer));
    }
}

// Function to read from server
void read_from_server(int sock)
{
    long valread = 0;
    long total = 0;
    char buffer[DIR_BUFFER];
    while ((valread = recv(sock, buffer, sizeof(buffer), 0)) > 0)
    {

        int len = strlen(buffer);

        total += len;
        if (strcmp(buffer + len - 4, "exit") == 0)
        {
            buffer[len - 4] = '\0';
            printf("%s", buffer);
            break;
        }
        else
        {
            printf("%s", buffer);
        }

        memset(buffer, 0, sizeof(buffer));
    }
    printf("Total read: %d\n", total);
}

// Function to validate dirlist command
bool validateDirlist(const char *input)
{
    if (strcmp(input, "dirlist -a") == 0 || strcmp(input, "dirlist -t") == 0)
    {
        return true;
    }
    return false;
}

// Function to validate w24fn command
bool validateW24fn(const char *input)
{
    // Assuming filename cannot contain spaces
    if (strstr(input, "w24fn ") == input && strchr(input + 6, ' ') == NULL)
    {
        return true;
    }
    return false;
}

// Function to validate w24fz command
bool validateW24fz(const char *input)
{
    long size1, size2;
    if (sscanf(input, "w24fz %ld %ld", &size1, &size2) == 2 && size1 >= 0 && size2 >= 0 && size1 <= size2)
    {
        return true;
    }
    return false;
}

// Function to validate w24ft command
bool validateW24ft(const char *input)
{
    // Check if the command starts with "w24ft"
    if (strstr(input, "w24ft ") == input)
    {
        // Count the number of space-separated arguments
        int count = 0;
        const char *ptr = input + 6; // Skip "w24ft "
        while (*ptr != '\0')
        {
            if (*ptr == ' ')
            {
                count++;
                ptr++; // Skip the space
            }
            else
            {
                ptr++; // Move to the next character
            }
        }
        count++; // Increment to account for the last argument

        // Check if the number of arguments is within the range [1, 3]
        if (count >= 1 && count <= 3)
        {
            return true;
        }
    }
    return false;
}

// Function to validate if a given date is valid
bool isValidDate(int year, int month, int day)
{
    if (year < 0 || month < 1 || month > 12 || day < 1)
    {
        return false;
    }

    int daysInMonth[] = {31, 28 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    return day <= daysInMonth[month - 1];
}

// Function to validate w24fdb command
bool validateW24fdb(const char *input)
{
    // Date format: yyyy-mm-dd
    int year, month, day;
    if (sscanf(input, "w24fdb %d-%d-%d", &year, &month, &day) == 3 && isValidDate(year, month, day))
    {
        return true;
    }
    return false;
}

// Function to validate w24fda command
bool validateW24fda(const char *input)
{
    // Date format: yyyy-mm-dd
    int year, month, day;
    if (sscanf(input, "w24fda %d-%d-%d", &year, &month, &day) == 3 && isValidDate(year, month, day))
    {
        return true;
    }
    return false;
}

// Function to validate quitc command
bool validateQuitc(const char *input)
{
    if (strcmp(input, "quitc") == 0)
    {
        return true;
    }
    return false;
}

// Function to validate all commands
bool validateCommands(const char *input)
{
    if (validateDirlist(input) || validateW24fn(input) || validateW24fz(input) || validateW24ft(input) || validateW24fdb(input) || validateW24fda(input) || validateQuitc(input))
    {
        return true;
    }
    return false;
}

// Main function
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Enter IP address\n");
    }

    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }

    int server_port;
    int bytes_received = read(sock, &server_port, sizeof(server_port));
    // printf("First thing: %d\t bytes: %d\n", server_port, bytes_received);

    if (server_port > 0)
    {
        close(sock);
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("Socket creation error");
            exit(EXIT_FAILURE);
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(server_port);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0)
        {
            perror("Invalid address/ Address not supported");
            exit(EXIT_FAILURE);
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("Connection Failed");
            exit(EXIT_FAILURE);
        }

        // printf("Connected to mirror %d\n", server_port);
    }

    const char *home_dir = getenv("HOME");
    const char *dir_name = "w24project";

    char full_path[BUFFER];
    snprintf(full_path, sizeof(full_path), "%s/%s", home_dir, dir_name);

    mkdir(full_path, 0777);

    char full_path_tar[BUFFER];

    snprintf(full_path_tar, sizeof(full_path_tar), "%s/%s/%s", home_dir, dir_name, "temp.tar.gz");


    while (1)
    {

        char command[BUFFER];
        printf(SHELL_MESSAGE);
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0'; // Remove newline character

        if (!validateCommands(command))
        {
            printf("Invalid command\n");
            memset(command, 0, sizeof(command));
            continue;
        }

        // Send command to server
        if (strlen(command) > 0)
        {
            send(sock, command, strlen(command), 0);
            // Check for termination command from server
            if (strcmp(command, "quitc") == 0)
            {
                printf("Terminating client process...\n");
                break;
            }
            memset(command, 0, sizeof(command));
        }

        // Receive response from server
        char buffer[BUFFER] = {0};
        int valread = read(sock, buffer, sizeof(buffer)-1);
        buffer[valread] = '\0'; // Null-terminate the received data

        // printf("From Server: %s\nSize: %d\n", buffer, strlen(buffer));

        // Check if the server is sending a tar file
        if (strcmp(buffer, "start_tar") == 0)
        {
            // Receive the file size from the server
            long file_size;
            int bytes_received = read(sock, &file_size, sizeof(file_size));
            // printf("File size: %d\n", file_size);
            if (bytes_received != sizeof(file_size))
            {
                perror("Error receiving file size");
                exit(EXIT_FAILURE);
            }

            // Receive and save the tar file from the server
            receive_tar(sock, file_size, full_path_tar);
            memset(buffer, 0, sizeof(buffer));
            memset(command, 0, sizeof(command));
            continue;
        }
        else if (strcmp(buffer, "dirlist") == 0)
        {
            long file_size;
            int bytes_received = read(sock, &file_size, sizeof(file_size));

            // printf("Dirlist -a %d\n", file_size);
            receive_text(sock, file_size);
            // read_from_server(sock);
            memset(buffer, 0, sizeof(buffer));
            memset(command, 0, sizeof(command));
            continue;
        }
        else
        {
            printf("%s\n", buffer);
            memset(buffer, 0, sizeof(buffer));
            memset(command, 0, sizeof(command));
        }
        memset(buffer, 0, sizeof(buffer));
        memset(command, 0, sizeof(command));
    }

    close(sock);
    return 0;
}
