#include <arpa/inet.h> // This header file provides functions for handling IP addresses and network addresses.
#include <stdio.h> // This C standard input/output library is used for input and output operations.
#include <stdlib.h> // This library provides functions for memory allocation, process control, conversions, and other operations.
#include <string.h> // This library provides functions for manipulating strings, such as copy, concatenate, and compare.
#include <sys/socket.h> // This header file defines types and functions for socket programming, which are used to create network sockets and communicate over them.
#include <sys/stat.h> // This header file provides functions for obtaining information about files (such as size, permissions, etc.).
#include <sys/types.h> // This header file defines various data types used in system calls and other system-related operations.
#include <unistd.h> // This header file provides access to the POSIX operating system API, which includes file operations, process management, and others.

// Defining constants and ports
#define PORT 6999                   // Main server port
#define MIRROR_PORT_1 7000          // Port number for mirror server 1
#define MIRROR_PORT_2 7001          // Port number for mirror server 2
#define GZIP_FILENAME "temp.tar.gz" // Expected gzip compressed file name
#define MAX_BUFFER_SIZE 1024
int validCommand = 0;

// Function to check if a file extension is supported
int isValidExtension(const char *extension) {
  const char *extensions[] = {"py",  "c",   "sh",  "txt",
                              "jpg", "pdf", "png", "jpeg"};
  for (int i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
    if (strcmp(extension, extensions[i]) == 0) {
      return 1; // Extension is supported
    }
  }
  return 0; // Extension is not supported
}

// Function to recieve file sent from the server
void receive_file(int server_socket) {
  char *get_home_dir = getenv("HOME"); // Get the HOME environment
                                       // variable
  if (get_home_dir == NULL) {
    perror("Failed to get home directory");
    exit(EXIT_FAILURE);
  }
  char w24_folder_path[1024];
  snprintf(w24_folder_path, sizeof(w24_folder_path), "%s/w24",
           get_home_dir); // Construct the folder path to w24

  // Check if the w24 folder exists, if not, creating it
  struct stat st = {0};
  if (stat(w24_folder_path, &st) == -1) {
    mkdir(w24_folder_path, 0700); // Create the directory with read, write, and
                                  // execute permissions for the owner
  }
  char targz_path[1024];
  snprintf(targz_path, sizeof(targz_path), "%s/%s", w24_folder_path,
           GZIP_FILENAME); // Construct the full file path

  FILE *file = fopen(targz_path, "wb");
  if (file == NULL) {
    perror("Error opening gzip file");
    exit(EXIT_FAILURE);
  }

  char size_buffer[sizeof(long)];
  ssize_t bytes_received = recv(server_socket, size_buffer, sizeof(long), 0);
  if (bytes_received != sizeof(long)) {
    perror("Failed to receive gzip file size");
    fclose(file);
    exit(EXIT_FAILURE);
  }

  long gzip_size;
  memcpy(&gzip_size, size_buffer, sizeof(long));

  char buffer[MAX_BUFFER_SIZE];
  size_t total_received = 0;
  while (total_received < gzip_size) {
    bytes_received = recv(server_socket, buffer, sizeof(buffer),
                          0); // recieve all the bytes sent from the server
    if (bytes_received < 0) { // Handle error if no bytes were received
      perror("Failed to receive data");
      fclose(file);
      exit(EXIT_FAILURE);
    }
    // write bytes if they were received sucessfully
    fwrite(buffer, 1, bytes_received, file);
    total_received += bytes_received;
  }

  fclose(file);
  printf("File %s received successfully and saved in %s\n", GZIP_FILENAME,
         w24_folder_path);
}

// Function used to parse the request send by the user
void parse_request(char *buff, int *rf, char *command) {
  // Duplicate the input string - for maintaining originality of buffer
  char user_input[1024];
  strcpy(user_input, buff);
  char *token = strtok(user_input, " ");
  *rf = 0; // Reset the file receptor flag

  if (token == NULL) {
    strcpy(command, ""); // Clear command if no input
    return;
  }

  /*Prints list of folders*/
  if (strcmp(token, "dirlist") == 0) {
    char *arg = strtok(NULL, " ");
    if (strcmp(arg, "-a") == 0) {
      /*List of subdirectories in alphabetical order*/
      sprintf(command, "dirlist %s", arg);
      validCommand = 1;
    } else if (strcmp(arg, "-t") == 0) {
      /*List of subdirectories in creation order - Oldest First*/
      sprintf(command, "dirlist %s", arg);
      validCommand = 1;
    } else {
      strcpy(command, "");
      // printf("Invalid dirlist extensions. Please try again\n");
      validCommand = 0;
    }
  }

  /*Fetch file details from filename such as name, size, date created,
   * permissions*/
  if (strcmp(token, "w24fn") == 0) {
    char *filename = strtok(NULL, " ");
    if (filename == NULL) {
      strcpy(command, "");
      validCommand = 0;
    } else {
      sprintf(command, "w24fn %s", filename);
      validCommand = 1;
    }
  }

  /*Tar with files whose size is size1 <= fileSize <= size2*/
  if (strcmp(token, "w24fz") == 0) {
    char *size1 = strtok(NULL, " ");
    char *size2 = strtok(NULL, " ");
    if (size1 == NULL || size2 == NULL || atoi(size1) < 0 || atoi(size2) < 0 ||
        atoi(size1) > atoi(size2)) {
      strcpy(command, "");
      validCommand = 0;
    } else {
      sprintf(command, "w24fz %s %s", size1, size2);
      validCommand = 1;
      // *rf = 1;  // Set flag to receive a file after this command
    }
  }

  /*Tar with files (with filetypes) as in the extension list specified by
   * request*/
  if (strcmp(token, "w24ft") == 0) {
    char *file_extension1 = strtok(NULL, " ");
    char *file_extension2 = strtok(NULL, " ");
    char *file_extension3 = strtok(NULL, " ");

    if (file_extension1 == NULL) {
      strcpy(command, "");
      validCommand = 0; // Invalid syntax - the extension list is empty
    } else {
      // Check if each extension is supported
      if (!isValidExtension(file_extension1) ||
          (file_extension2 && !isValidExtension(file_extension2)) ||
          (file_extension3 && !isValidExtension(file_extension3))) {
        printf("Invalid file extension provided.\n");
        return;
      }

      // Construct the command with up to 3 different file types
      sprintf(command, "w24ft %s", file_extension1);
      if (file_extension2 != NULL) {
        sprintf(command + strlen(command), " %s", file_extension2);
      }
      if (file_extension3 != NULL) {
        sprintf(command + strlen(command), " %s", file_extension3);
      }
      validCommand = 1;
    }
  }

  /*Tar with files created after the requested date*/
  if (strcmp(token, "w24fda") == 0) {
    char *gdate = strtok(NULL, " ");
    if (gdate == NULL) {
      strcpy(command, "");
      validCommand = 0; // Clear command if invalid input
    } else {
      sprintf(command, "w24fda %s", gdate);
      validCommand = 1;
      *rf = 1; // Set flag to receive a file after this command
    }
  }

  /*Tar with files created before the requested date*/
  if (strcmp(token, "w24fdb") == 0) {
    char *ldate = strtok(NULL, " ");
    if (ldate != NULL) {
      sprintf(command, "w24fdb %s", ldate);
      *rf = 1; // Set flag to receive a file after this command
      validCommand = 1;
    } else {
      strcpy(command, "");
      validCommand = 0; // Clear command if invalid input
    }
  } else {
    strcpy(command, buff); // Use the raw buffer as command otherwise
  }
}

// main function to establish connection with server
int main() {
  int sockfd;
  struct sockaddr_in serv_addr, mirror_addr;
  char buff[1024], command[1024];
  int rf = 0; // Flag indicating if file reception is expected

  // setup of socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error creating socket");
    exit(EXIT_FAILURE);
  }
  // initializing addresses
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    perror("Invalid IP address / Address not supported");
    close(sockfd);
    return -1;
  }
  // connecting with the server
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("Connection failed - Server NOT connected");
    close(sockfd);
    return -1;
  }

  printf("Connected to the server!\n");
  // after connections ask for user inputs
  while (1) {
    printf("Enter a command or 'quitc' to exit:\n"); // Prompt user for input
    printf("clientw24$ ");
    memset(buff, 0, sizeof(buff));
    fgets(buff, sizeof(buff), stdin);
    buff[strcspn(buff, "\n")] = 0; // Remove newline character

    if (strcmp(buff, "quitc") == 0) {
      break;
    }

    parse_request(buff, &rf, command);
    // printf("length of command is:%lu\n", strlen(command));
    //  if (strlen(command) == 0) {
    //    printf("Invalid command or syntax. Please try again.\n");
    //    continue;
    //  }
    if (validCommand == 0) {
      printf("Invalid command or syntax. Please try again.\n \n");
      continue;
    }

    send(sockfd, command, strlen(command), 0);
    if (rf) {
      receive_file(sockfd);
    } else {
      char response[1024] = {0};
      int bytes_read = read(sockfd, response, sizeof(response));
      // response[bytes_read] = '\0';  // Ensure null termination
      if (strncmp(response, "REDIRECT:", 9) != 0) {
        printf("%s\n", response);
      }

      // Check if the response is a redirection
      if (strncmp(response, "REDIRECT:", 9) == 0) {
        int new_port = atoi(response + 9); // Extract the new port number
        close(sockfd); // Close the current connection - switching to mirrors

        // Creates new socket for the mirror server
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
          perror("socket");
          exit(EXIT_FAILURE);
        }

        memset(&mirror_addr, '\0', sizeof(mirror_addr));
        mirror_addr.sin_family = AF_INET;
        mirror_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        // Connect to the mirror server
        if (new_port == 7000) {
          mirror_addr.sin_port = htons(MIRROR_PORT_1);
        } else {
          mirror_addr.sin_port = htons(MIRROR_PORT_2);
        }

        if (connect(sockfd, (struct sockaddr *)&mirror_addr,
                    sizeof(mirror_addr)) == -1) {
          perror("connect");
          exit(EXIT_FAILURE);
        }

        if (new_port == 7000 || new_port == 7001) {
          // Send the original command to the new server (mirror1)
          send(sockfd, command, strlen(command), 0);

          // Read the response from the new server
          memset(response, 0, sizeof(response));
          bytes_read = read(sockfd, response, sizeof(response));
          printf("%s\n", response);
        }
      }
    }
  } // end-while

  close(sockfd);
  printf("Connection closed.\n");
  return 0;
}
