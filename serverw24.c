#define _XOPEN_SOURCE 1 /* Required under GLIBC for nftw() */
#define _GNU_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1

// necessary libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#include <ftw.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

// necessary defines
#define PORT 5151
#define MAX_DIRS 10240
#define MAX_DIR_NAME 512
#define BUFFER 1024
#define SERVER_ID 1
#define MIRROR1_ID 2
#define MIRROR2_ID 3
#define MIRROR1_PORT 6161
#define MIRROR2_PORT 7171
#define HOME "HOME"
#define DIRLIST "dirlist"

// sructure to store directory information
struct dir_info
{
    char name[MAX_DIR_NAME];
    time_t creation_time;
    int depth;
};
// client counter
static int client_count = 0;
static int flag = 0;

// sending error message
void send_erorr_message(int client_socket, const char *msg)
{
    send(client_socket, msg, strlen(msg), 0);
}
// sending success message
void send_success_message(int client_socket, const char *msg)
{
    send(client_socket, msg, strlen(msg), 0);
}
// compare function for qsort for dirlist -a
int compare(const void *a, const void *b)
{
    return strcmp((char *)a, (char *)b);
}
// compare function for qsort for dirlist -t
int comparetime(const void *a, const void *b)
{
    const struct dir_info *dir1 = (const struct dir_info *)a;
    const struct dir_info *dir2 = (const struct dir_info *)b;
    if (dir1->depth < dir2->depth)
    {
        return -1; // dir1 has lower depth, should appear before dir2
    }
    else if (dir1->depth > dir2->depth)
    {
        return 1; // dir1 has higher depth, should appear after dir2
    }
    else
    {
        // If depths are equal, compare creation time
        if (dir1->creation_time < dir2->creation_time)
        {
            return -1; // dir1 is older, should appear before dir2
        }
        else if (dir1->creation_time > dir2->creation_time)
        {
            return 1; // dir1 is newer, should appear after dir2
        }
        else
        {
            return 0; // Same creation time, no preference
        }
    }
}
// list directories for dirlist command
int list_dirs_(const char *name, const struct stat *s, int type, struct FTW *f)
{
    if (type == FTW_D)
    {
        printf("%s\n", name);
    }
    return 0;
}
// This function handles the 'dirlist' command from a client.
// It takes in a client socket and an option as parameters.
void handle_dirlist_command(int client_socket, const char *option)
{
    // Get the path to the home directory
    char *path = getenv(HOME);
    // char *path = "/home/patel2y8/Desktop";

    // Send the 'dirlist' command to the client
    send(client_socket, "dirlist", strlen("dirlist"), 0);
    sleep(1);

    // If the option is '-a', list all directories
    if (strcmp(option, "-a") == 0)
    {
        // Array to store directory names
        char *directories[MAX_DIRS];
        int count = 0;

        // Function to list directories
        int list_dirs(const char *name, const struct stat *s, int type, struct FTW *f)
        {
            // If the file type is a directory
            if (type == FTW_D)
            {
                // Allocate memory for the directory name and add it to the array
                directories[count] = malloc(strlen(name) + 2);
                strcpy(directories[count], name);
                strcat(directories[count], "\n");
                count++;
            }
            return 0;
        }

        // Traverse the file system starting from the path
        if (nftw(path, list_dirs, 20, 0) != 0)
        {
            perror("nftw");
            return;
        }

        // Sort the directory names in alphabetical order
        qsort(directories, count, sizeof(char *), compare);

        long total_bytes = 0;
        // Calculate the total size of the directory names
        for (int i = 0; i < count; i++)
        {
            total_bytes += strlen(directories[i]);
        }
        // Send the total size to the client
        send(client_socket, &total_bytes, sizeof(total_bytes), 0);

        // Send the sorted directory names to the client
        for (int i = 0; i < count; i++)
        {
            write(client_socket, directories[i], strlen(directories[i]));
        }

        return;
    }

    // If the option is '-t', list directories sorted by creation time
    if (strcmp(option, "-t") == 0)
    {
        // Array to store directory info
        struct dir_info dirs[MAX_DIRS];
        int dir_count = 0;

        // Function to list directories
        int list_dirs(const char *name, const struct stat *s, int type, struct FTW *f)
        {
            // If the file type is a directory
            if (type == FTW_D)
            {
                // If the number of directories is less than the maximum
                if (dir_count < MAX_DIRS)
                {
                    // Store the directory name and creation time
                    strcpy(dirs[dir_count].name, name);
                    strcat(dirs[dir_count].name, "\n");
                    dirs[dir_count].creation_time = s->st_ctime;
                    dir_count++;
                }
                else
                {
                    // If the maximum number of directories is exceeded, abort traversal
                    fprintf(stderr, "Maximum directories exceeded\n");
                    return 1;
                }
            }
            return 0;
        }

        // Traverse the file system starting from the path
        if (nftw(path, list_dirs, 20, 0) != 0)
        {
            perror("nftw");
            return;
        }

        // Sort directories based on creation time
        qsort(dirs, dir_count, sizeof(struct dir_info), comparetime);

        long t_bytes = 0;
        // Calculate the total size of the directory names
        for (int i = 0; i < dir_count; i++)
        {
            t_bytes += strlen(dirs[i].name);
        }
        // Send the total size to the client
        send(client_socket, &t_bytes, sizeof(t_bytes), 0);

        // Send the sorted directory names to the client
        for (int i = 0; i < dir_count; i++)
        {
            write(client_socket, dirs[i].name, strlen(dirs[i].name));
        }
    }
}

//  Function to convert file permissions to octal
int octal(mode_t mode)
{
    return (mode & 0777);
}

// Function to chekc if file found or not
void filename_found(const char *filename)
{
    printf("%s", filename);
    if (strcmp(filename, "\0") == 0)
    {
        printf("File not found: %s", filename);
    }
}
// Function to handle the w24fn command
void handle_w24fn_command(int client_socket, const char *filename)
{
    char command[BUFFER]; // Command buffer

    // Construct the find command to search for the file
    snprintf(command, sizeof(command), "find ~ -name '%s' -print -quit", filename);

    // Open a pipe to read the output of the find command
    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        perror("popen");
        return;
    }

    char file_path[BUFFER]; // Buffer to store the found file path

    // Read the output of the find command
    if (fgets(file_path, sizeof(file_path), fp) != NULL)
    {
        // Remove trailing newline character from file path
        file_path[strcspn(file_path, "\n")] = '\0';

        // File found, get its information
        struct stat file_stat;
        if (stat(file_path, &file_stat) == -1)
        {
            // Error accessing file
            perror("stat");
            return;
        }
        // char *folder_name = basename(file_path);
        // Prepare file information message
        char file_info[BUFFER]; // Adjust size as needed
        snprintf(file_info, sizeof(file_info), "Filename: %s\nSize: %ld bytes\nDate Created: %sPermissions: %c%c%c%c%c%c%c%c%c%c (Octal: 0%o)\n",
                 file_path, file_stat.st_size, ctime(&file_stat.st_ctime),
                 (S_ISDIR(file_stat.st_mode)) ? 'd' : '-',
                 (file_stat.st_mode & S_IRUSR) ? 'r' : '-',
                 (file_stat.st_mode & S_IWUSR) ? 'w' : '-',
                 (file_stat.st_mode & S_IXUSR) ? 'x' : '-',
                 (file_stat.st_mode & S_IRGRP) ? 'r' : '-',
                 (file_stat.st_mode & S_IWGRP) ? 'w' : '-',
                 (file_stat.st_mode & S_IXGRP) ? 'x' : '-',
                 (file_stat.st_mode & S_IROTH) ? 'r' : '-',
                 (file_stat.st_mode & S_IWOTH) ? 'w' : '-',
                 (file_stat.st_mode & S_IXOTH) ? 'x' : '-',
                 octal(file_stat.st_mode));
        // Send file information to client
        send(client_socket, file_info, strlen(file_info), 0);
    }
    else
    {
        // File not found
        char error_msg[100];
        snprintf(error_msg, sizeof(error_msg), "File not found: %s", filename);
        send(client_socket, error_msg, strlen(error_msg), 0);
    }

    // Close the pipe
    pclose(fp);
}

// w24fz function prototypes
//  Function to create a tarball for files within a specified size range
void create_tar_for_w24fz(int client_socket, int lower_limit, int upper_limit)
{
    char cmd[BUFFER];
    char filename[BUFFER];
}

// Function to handle the w24fz command
void handle_w24fz_command(int client_socket, long size1, long size2)
{

    char buffer[BUFFER] = {0};
    int n;
    char cmd[BUFFER];
    char filename[BUFFER];
    char temp_tar_name[BUFFER];

    // Generate a unique filename based on the current time
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(filename, sizeof(filename), "file_%Y%m%d%H%M%S.txt", tm);
    strftime(temp_tar_name, sizeof(temp_tar_name), "temp_%Y%m%d%H%M%S.tar.gz", tm);

    // Construct the find command to locate files within the specified size range
    sprintf(cmd, "find ~ -type f -size +%ldc -size -%ldc > %s", size1, size2, filename);
    system(cmd);

    // Verify if the file is empty
    FILE *file = fopen(filename, "r");
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    // fclose(file);

    if (filesize == 0)
    {
        printf("No file found\n");
        send(client_socket, "No file found", sizeof("No file found"), 0);
        remove(filename);
        return;
    }
    else
    {
        // Generate a tarball using the files listed in the file
        sprintf(cmd, "tar -czf %s -T %s", temp_tar_name, filename);
        system(cmd);
    }

    printf("\nTar created.\n");

    // Open the tar file
    FILE *tar_fp = fopen(temp_tar_name, "rb");
    if (tar_fp == NULL)
    {
        perror("fopen");
        send(client_socket, "File not found", sizeof("File not found"), 0);
        remove(filename);
        return;
    }

    // Calculate the size of the tar file
    fseek(tar_fp, 0, SEEK_END);
    long file_size = ftell(tar_fp);
    rewind(tar_fp);

    printf("File Size: %d\n", file_size);
    // Send the "start_tar" message to the client
    send(client_socket, "start_tar", sizeof("start_tar"), 0);
    sleep(1);
    // Send the size of the tar file to the client
    ssize_t bytes_sent = send(client_socket, &file_size, sizeof(file_size), 0);
    if (bytes_sent != sizeof(file_size))
    {
        perror("Error sending file size");
        exit(EXIT_FAILURE);
    }

    memset(buffer, 0, sizeof(buffer));

    // Read and send the entire tar file to the client
    while (1)
    {
        ssize_t bytes_read = fread(buffer, 1, sizeof(buffer), tar_fp);
        if (bytes_read <= 0)
        {
            if (feof(tar_fp))
            {
                // End of file reached
                break;
            }
            else
            {
                perror("Error reading tar file");
                exit(EXIT_FAILURE);
            }
        }

        // Send the read data to the client
        ssize_t bytes_written = write(client_socket, buffer, bytes_read);
        if (bytes_written != bytes_read)
        {
            perror("Error sending tar data");
            exit(EXIT_FAILURE);
        }
    }

    // Close the tar file
    fclose(tar_fp);

    remove(temp_tar_name);
    remove(filename);

    return;
}

// w24ft function prototypes
//  Function to handle the w24ft command
void handle_w24ft_command(int client_socket, char *extensions[], int num_extensions)
{
    // Construct the find command
    char find_command[BUFFER];
    char filename[BUFFER];
    char temp_tar_name[BUFFER];

    // Generate a unique filename based on the current time
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(filename, sizeof(filename), "file_%Y%m%d%H%M%S.txt", tm);
    strftime(temp_tar_name, sizeof(temp_tar_name), "temp_%Y%m%d%H%M%S.tar.gz", tm);

    snprintf(find_command, sizeof(find_command), "find ~ -type f \\( ");
    for (int i = 0; i < num_extensions; ++i)
    {
        strncat(find_command, "-iname '*.", sizeof(find_command) - strlen(find_command) - 1);
        strncat(find_command, extensions[i], sizeof(find_command) - strlen(find_command) - 1);
        strncat(find_command, "' ", sizeof(find_command) - strlen(find_command) - 1);
        if (i != num_extensions - 1)
        {
            strncat(find_command, "-o ", sizeof(find_command) - strlen(find_command) - 1);
        }
    }
    strncat(find_command, "\\) -print", sizeof(find_command) - strlen(find_command) - 1);

    // Open a pipe to read the output of the find command
    FILE *fp = popen(find_command, "r");
    if (fp == NULL)
    {
        perror("popen");
        send(client_socket, "Error executing find command", sizeof("Error executing find command"), 0);
        return;
    }

    // Open a temporary file to store the list of found files
    FILE *temp_fp = fopen(filename, "w");
    if (temp_fp == NULL)
    {
        perror("fopen");
        send(client_socket, "Error creating temporary file", sizeof("Error creating temporary file"), 0);
        pclose(fp);
        return;
    }

    // Read the output of the find command and write to the temporary file
    char buffer[BUFFER];
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        fprintf(temp_fp, "%s", buffer);
    }

    // Close the pipe and temporary file
    pclose(fp);
    fclose(temp_fp);

    // Check if any files were found
    temp_fp = fopen(filename, "rb");

    if (temp_fp == NULL)
    {
        send(client_socket, "No file found", sizeof("No file found"), 0);
        remove(filename);
        return;
    }

    // Seek to the end of the file and get the file size
    fseek(temp_fp, 0L, SEEK_END);
    long size = ftell(temp_fp);

    // Check if the file is empty
    if (size == 0)
    {
        send(client_socket, "No file found", sizeof("No file found"), 0);
        remove(filename);
        return;
    }

    // Create a compressed tar archive
    char tar_command[BUFFER];
    snprintf(tar_command, sizeof(tar_command), "tar -czf %s -T %s", temp_tar_name, filename);
    int ret = system(tar_command);
    if (ret != 0)
    {
        perror("system");
        send(client_socket, "Error creating archive", sizeof("Error creating archive"), 0);
        remove(filename);
        return;
    }
    else
    {
        printf("Tar file created successfully.\n");
    }

    // Open the tar file
    FILE *tar_fp = fopen(temp_tar_name, "rb");
    if (tar_fp == NULL)
    {
        perror("fopen");
        send(client_socket, "No file found", sizeof("No file found"), 0);
        remove(filename);
        return;
    }

    // Send the "start_tar" message to the client
    send(client_socket, "start_tar", sizeof("start_tar"), 0);
    sleep(1);
    // Calculate the size of the tar file
    fseek(tar_fp, 0, SEEK_END);
    long file_size = ftell(tar_fp);
    rewind(tar_fp);

    // Send the size of the tar file to the client
    ssize_t bytes_sent = send(client_socket, &file_size, sizeof(file_size), 0);
    if (bytes_sent != sizeof(file_size))
    {
        perror("Error sending file size");
        exit(EXIT_FAILURE);
    }

    // Read and send the entire tar file to the client
    memset(buffer, 0, sizeof(buffer));

    // Read and send the entire tar file to the client
    while (1)
    {
        ssize_t bytes_read = fread(buffer, 1, sizeof(buffer), tar_fp);
        if (bytes_read <= 0)
        {
            if (feof(tar_fp))
            {
                // End of file reached
                break;
            }
            else
            {
                perror("Error reading tar file");
                exit(EXIT_FAILURE);
            }
        }

        // Send the read data to the client
        ssize_t bytes_written = write(client_socket, buffer, bytes_read);
        if (bytes_written != bytes_read)
        {
            perror("Error sending tar data");
            exit(EXIT_FAILURE);
        }
    }

    // Close the tar file
    fclose(tar_fp);

    // Remove the temporary files
    remove(temp_tar_name);
    remove(filename);
}

// Function to handle w24fdb command
void handle_w24fdb_command(int client_socket, const char *date)
{
    if (date == NULL)
    {
        send(client_socket, "Error: No date provided", sizeof("Error: No date provided"), 0);
        return;
    }

    char temp_tar_name[BUFFER];

    // Generate a unique filename based on the current time
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(temp_tar_name, sizeof(temp_tar_name), "temp_%Y%m%d%H%M%S.tar.gz", tm);

    // Construct the find command
    char find_command[BUFFER];
    snprintf(find_command, sizeof(find_command), "find ~ -type f -newermt '%s' -exec tar -czf %s {} +", date, temp_tar_name);

    // Execute the find command
    int ret = system(find_command);
    if (ret != 0)
    {
        perror("system");
        send(client_socket, "Error executing find command", sizeof("Error executing find command"), 0);
        return;
    }

    // Open the tar file
    FILE *tar_fp = fopen(temp_tar_name, "rb");
    if (tar_fp == NULL)
    {
        perror("fopen");
        send(client_socket, "File not found", sizeof("File not found"), 0);
        return;
    }

    // Send the "start_tar" message to the client
    send(client_socket, "start_tar", sizeof("start_tar"), 0);
    sleep(1);

    printf("Tar created.");

    // Calculate the size of the tar file
    fseek(tar_fp, 0, SEEK_END);
    long file_size = ftell(tar_fp);
    rewind(tar_fp);

    // Send the size of the tar file to the client
    ssize_t bytes_sent = send(client_socket, &file_size, sizeof(file_size), 0);
    if (bytes_sent != sizeof(file_size))
    {
        perror("Error sending file size");
        exit(EXIT_FAILURE);
    }

    // Read and send the entire tar file to the client
    char buffer[BUFFER] = {0};
    while (1)
    {
        ssize_t bytes_read = fread(buffer, 1, sizeof(buffer), tar_fp);
        if (bytes_read <= 0)
        {
            if (feof(tar_fp))
            {
                // End of file reached
                break;
            }
            else
            {
                perror("Error reading tar file");
                exit(EXIT_FAILURE);
            }
        }

        // Send the read data to the client
        ssize_t bytes_written = write(client_socket, buffer, bytes_read);
        if (bytes_written != bytes_read)
        {
            perror("Error sending tar data");
            exit(EXIT_FAILURE);
        }
    }

    // Close the tar file
    fclose(tar_fp);

    // Remove the temporary tar file
    remove(temp_tar_name);
}

// Function to handle w24fda command
void handle_w24fda_command(int client_socket, const char *date)
{
    if (date == NULL)
    {
        send(client_socket, "Error: No date provided", sizeof("Error: No date provided"), 0);
        return;
    }

    char temp_tar_name[BUFFER];

    // Generate a unique filename based on the current time
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(temp_tar_name, sizeof(temp_tar_name), "temp_%Y%m%d%H%M%S.tar.gz", tm);
    // Construct the find command
    char find_command[BUFFER];
    snprintf(find_command, sizeof(find_command), "find ~ -type f ! -newermt '%s' -exec tar -czf %s {} +", date, temp_tar_name);

    // Execute the find command
    int ret = system(find_command);
    if (ret != 0)
    {
        perror("system");
        send(client_socket, "Error executing find command", sizeof("Error executing find command"), 0);
        return;
    }

    // Open the tar file
    FILE *tar_fp = fopen(temp_tar_name, "rb");
    if (tar_fp == NULL)
    {
        perror("fopen");
        send(client_socket, "File not found", sizeof("File not found"), 0);
        return;
    }

    // Send the "start_tar" message to the client
    send(client_socket, "start_tar", sizeof("start_tar"), 0);
    sleep(1);

    printf("Tar created.");

    // Calculate the size of the tar file
    fseek(tar_fp, 0, SEEK_END);
    long file_size = ftell(tar_fp);
    rewind(tar_fp);

    // Send the size of the tar file to the client
    ssize_t bytes_sent = send(client_socket, &file_size, sizeof(file_size), 0);
    if (bytes_sent != sizeof(file_size))
    {
        perror("Error sending file size");
        exit(EXIT_FAILURE);
    }

    // Read and send the entire tar file to the client
    char buffer[BUFFER] = {0};
    while (1)
    {
        ssize_t bytes_read = fread(buffer, 1, sizeof(buffer), tar_fp);
        if (bytes_read <= 0)
        {
            if (feof(tar_fp))
            {
                // End of file reached
                break;
            }
            else
            {
                perror("Error reading tar file");
                exit(EXIT_FAILURE);
            }
        }

        // Send the read data to the client
        ssize_t bytes_written = write(client_socket, buffer, bytes_read);
        if (bytes_written != bytes_read)
        {
            perror("Error sending tar data");
            exit(EXIT_FAILURE);
        }
    }

    // Close the tar file
    fclose(tar_fp);

    // Remove the temporary tar file
    remove(temp_tar_name);
}

// Function to load balance the connections
int load_balancer(int connections)
{
    if (connections >= 0 && connections < 3)
    {
        return SERVER_ID;
    }
    if (connections >= 3 && connections < 6)
    {
        return MIRROR1_ID;
    }
    if (connections >= 6 && connections < 9)
    {
        return MIRROR2_ID;
    }
    int alternation = connections % 3;

    if (alternation == 0)
    {
        return SERVER_ID;
    }
    if (alternation == 1)
    {
        return MIRROR1_ID;
    }
    if (alternation == 2)
    {
        return MIRROR2_ID;
    }
}

// Function to forward the client to the mirror server
void client_forward(int client_connection, int mirror_port)
{
    send(client_connection, &mirror_port, sizeof(mirror_port), 0);
    printf("Redirected...\n");
}

// Function to handle the client request
void crequest(int client_socket)
{
    int main_id = -1;
    send(client_socket, &main_id, sizeof(main_id), 0);

    while (1)
    {
        // Read command from the client
        char buffer[BUFFER] = {0};
        read(client_socket, buffer, BUFFER);
        printf("Received command: %s\n", buffer);

        // Parse and handle the command
        char *command = strtok(buffer, " ");
        if (strcmp(command, "dirlist") == 0)
        {
            char *option = strtok(NULL, " ");
            handle_dirlist_command(client_socket, option);
        }
        else if (strcmp(command, "w24fz") == 0)
        {
            char *size1 = strtok(NULL, " ");
            char *size2 = strtok(NULL, " ");
            handle_w24fz_command(client_socket, atol(size1), atol(size2));
        }
        if (strcmp(command, "w24fn") == 0)
        {
            char *filename = strtok(NULL, " ");
            handle_w24fn_command(client_socket, filename);
        }
        if (strcmp(command, "w24ft") == 0)
        {
            // Initialize an array to store the ext
            char *ext[3];
            int tot_ext = 0;

            // Tokenize the input string to extract ext
            char *token = strtok(NULL, " ");
            while (token != NULL && tot_ext < 3)
            {
                ext[tot_ext++] = token;
                token = strtok(NULL, " ");
            }

            // Call the function to handle the w24ft command with the extracted ext
            handle_w24ft_command(client_socket, ext, tot_ext);
        }
        else if (strcmp(command, "w24fdb") == 0)
        {
            char *date = strtok(NULL, " ");
            handle_w24fdb_command(client_socket, date);
        }
        else if (strcmp(command, "w24fda") == 0)
        {
            char *date = strtok(NULL, " ");
            handle_w24fda_command(client_socket, date);
        }
        else if (strcmp(command, "quitc") == 0)
        {
            // Terminate the client process
            exit(EXIT_SUCCESS);
        }
        memset(buffer, 0, sizeof(buffer));
    }
}

// Main function
int main()
{
    int server_fd, new_socket, status;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {

        printf("\nServer is running...\n");
        // For removing the zombie process
        waitpid(-1, NULL, WNOHANG);

        int server_id = load_balancer(client_count);
        client_count++;

        if (server_id == SERVER_ID)
        {
            new_socket = accept(server_fd, (struct sockaddr *)NULL, NULL);
            printf("\nGot a new client in Main\n");
            if (!fork()) // Child process
                crequest(new_socket);
            close(new_socket);
            waitpid(0, &status, WNOHANG);
        }
        else if (server_id == MIRROR1_ID)
        {
            new_socket = accept(server_fd, (struct sockaddr *)NULL, NULL);
            printf("\nGot a client to forward to Mirror 1\n");
            if (!fork())
            {
                client_forward(new_socket, MIRROR1_PORT);
                close(new_socket);
                exit(0);
            }

            close(new_socket);
            waitpid(0, &status, WNOHANG);
        }
        else if (server_id == MIRROR2_ID)
        {
            new_socket = accept(server_fd, (struct sockaddr *)NULL, NULL);
            printf("\nGot a client to forward to Mirror 2\n");
            if (!fork())
            {
                client_forward(new_socket, MIRROR2_PORT);
                close(new_socket);
                exit(0);
            }
            close(new_socket);
            waitpid(0, &status, WNOHANG);
        }
    }
    return 0;
}
