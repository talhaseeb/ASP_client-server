/*
* Client - Server Communication using Sockets (Mirror1 and Mirror2 - load balancing)
* Mirror1 - Server Connection - Port 7000
*
* Authors: Abdul Rahman Mohammed (110128321) & Talha Haseeb Mohammed (110128322)
*/

/*Libraries defined*/
#define _XOPEN_SOURCE 700  // Enables certain features in POSIX APIs - nftw PHYS Flag issues resolver
#include <arpa/inet.h>  // Provides functions for manipulating IP addresses
#include <dirent.h>  // Allows accessing directory entries
#include <fcntl.h>  // Provides file control options
#include <ftw.h>  // Offers file tree walk functionality
#include <libgen.h>  // Provides filename manipulation functions
#include <netinet/in.h>  // Defines internet address structures
#include <stdbool.h>  // Defines boolean data type and values
#include <stdio.h>  // Provides standard input/output functionality
#include <stdlib.h>  // Provides standard library functions
#include <string.h>  // Provides string manipulation functions
#include <sys/socket.h>  // Provides socket interface functions
#include <sys/stat.h>  // Provides functions for obtaining file status
#include <sys/types.h>  // Defines various data types used in system calls
#include <sys/wait.h>  // Provides functions for process control
#include <time.h>  // Provides time-related functions
#include <unistd.h>  // Provides various standard POSIX operating system functions
#include <limits.h>  // Defines system-specific constants for pathnames
#include <pwd.h>  // Provides functions for retrieving user information


// Global definitions (Ports/Buffer sizes)
#define SERVER_PORT 6999
#define MIRROR1_PORT 7000
#define MIRROR2_PORT 7001
#define BUFFER_SIZE 2048
#define MAX_DIRS 100
#define MAX_DIR_NAME_LEN 256
#define MAX_PATH_LEN 2560

char file_info[1024] = {0};
char *inputFileName;
char *file_list[1024];
int file_count = 0;
time_t date_limit;
char *homePath = "home/";

//char* tar_filename = "temp.tar";
char* gzip_filename = "temp.tar.gz";

/*Function: fetch errors and exit*/
void caught_error(const char *msg) {
  perror(msg);
  exit(1);
}

/*Function: Comparion for Qsort */
int compareStrings(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

/*Function: If w24 folder doesnot exist - create it*/
void create_w24_directory() {
    // Get the home directory path
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        perror("getenv");
        exit(EXIT_FAILURE);
    }

    // Concatenate the home directory path with the w24 directory name
    char w24_dir[1024];
    snprintf(w24_dir, sizeof(w24_dir), "%s/w24", home_dir);

    // Check if the w24 directory exists
    struct stat st;
    if (stat(w24_dir, &st) == -1) {
        // If the directory doesn't exist, create it
        if (mkdir(w24_dir, 0700) == -1) {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
    }
}

/*
*Command: dirlist -a
*/

/*Function: Returns list of folders(only) under ~ directory in the alphabetical order */
void dirlistA(char *response) {
  FILE *fp;
  char path[MAX_DIR_NAME_LEN];
  char *directories[MAX_DIRS];
  int count = 0;
  // Open the pipe to run the find command - fetch only folders belonging to user and not hidden files
  fp = popen("find ~/ -type d -not "
             "-path '*/.*' -user \"$(whoami)\"",
             "r");
  if (fp == NULL) {
    perror("popen");
    exit(EXIT_FAILURE);
  }
  // Read directory names from the pipe and store them in an array
  while (fgets(path, sizeof(path), fp) != NULL && count < MAX_DIRS) {
    // Remove newline character from directory name
    path[strcspn(path, "\n")] = '\0';
    // Extract only the directory name from the path
    char *directory_name = strrchr(path, '/');
    if (directory_name != NULL) {
      directory_name++; // Move past the '/'
      // Store the directory name if it's not empty
      if (*directory_name != '\0') {
        directories[count] = strdup(directory_name);
        count++;
      }
    }
  }
  // Close the pipe
  pclose(fp);
  // Sort the directory names alphabetically
  qsort(directories, count, sizeof(char *), compareStrings);
  // Prepare the response string
  sprintf(response, "Sorted list of sub-directories:\n");
  for (int i = 0; i < count; i++) {
    strcat(response, directories[i]);
    strcat(response, "\n");
    free(directories[i]); // Free the allocated memory
  }
}

/*
*Command: dirlist -t
*/

/* Function: Returns list folders(only) in the order of creation time -- oldest first (Wait -Die :)) */
void dirlistT(char *response) {
  FILE *fp;
  char path[MAX_DIR_NAME_LEN];
  char *directories[MAX_DIRS];
  int count = 0;
  // Run the find command - fetch folders belonging to user and not hidden files - sort based on bdate
  fp = popen("find ~/ -type d -not "
             "-path '*/.*' -user \"$(whoami)\" -exec stat "
             "--format '%W %n' {} + | sort -n | cut -d ' ' -f 2-",
             "r");
  if (fp == NULL) {
    perror("popen");
    exit(EXIT_FAILURE);
  }
  // Read directory paths from the pipe and extract directory names
  while (fgets(path, sizeof(path), fp) != NULL && count < MAX_DIRS) {
    // Remove newline character from directory path
    path[strcspn(path, "\n")] = '\0';
    // Extract only the directory name from the path
    char *directory_name = strrchr(path, '/');
    if (directory_name != NULL) {
      directory_name++; // Move past the '/'
      // Store the directory name if it's not empty
      if (*directory_name != '\0') {
        directories[count] = strdup(directory_name);
        count++;
      }
    }
  }
  // Close the pipe
  pclose(fp);
  // Prepare the response string
  sprintf(response,
          "List of Sub-directories in the order of creation time:\n");
  for (int i = 0; i < count; i++) {
    strcat(response, directories[i]);
    strcat(response, "\n");
    free(directories[i]); // Free allocated memory
  }
}

/*
*Command: w24fn
*/

/*Function: Retrieve file permissions without use of external libs - for w24fn (filename)*/
void extract_permissions(mode_t mode, char *permissions) {
  permissions[0] = (S_ISDIR(mode)) ? 'd' : '-';
  permissions[1] = (mode & S_IRUSR) ? 'r' : '-';
  permissions[2] = (mode & S_IWUSR) ? 'w' : '-';
  permissions[3] = (mode & S_IXUSR) ? 'x' : '-';
  permissions[4] = (mode & S_IRGRP) ? 'r' : '-';
  permissions[5] = (mode & S_IWGRP) ? 'w' : '-';
  permissions[6] = (mode & S_IXGRP) ? 'x' : '-';
  permissions[7] = (mode & S_IROTH) ? 'r' : '-';
  permissions[8] = (mode & S_IWOTH) ? 'w' : '-';
  permissions[9] = (mode & S_IXOTH) ? 'x' : '-';
  permissions[10] = '\0'; // end/null term
}

/*Function: Fetches filename*/
char *get_filename(const char *path) {
  const char *last_slash = strrchr(path, '/');
  if (last_slash) {
    return (char *)(last_slash +
                    1); // Return the substring after the last slash
  }
  return (char *)path; // If no slash was found, path is the filename
}

/* Callback Function for NFTW: Parsing directory structure -physical walks */
int file_processor(const char *fpath, const struct stat *sb, int typeflag,
                   struct FTW *ftwbuf) {
  if (typeflag == FTW_F) {
    char *target_filename = inputFileName; // Target file to search for
    if (strcmp(target_filename, fpath + ftwbuf->base) == 0) {
      // File found, extract details
      char permissions[11];
      extract_permissions(sb->st_mode, permissions);

      char creation_time[30];
      strftime(creation_time, sizeof(creation_time), "%Y-%m-%d %H:%M:%S",
               localtime(&sb->st_ctime));

      char *fname = get_filename(fpath);

      sprintf(file_info,
              "File: %s\nSize: %ld bytes\nDate created: %s\nPermissions: %s\n",
              fname, (long)sb->st_size, creation_time, permissions);
      return 1; // Stop the walk as file is found
    }
  }
  return 0; // Continue walking
}

/*Function: recursive callback (nftw) to search directory tree for filename*/
void w24fn(const char *root_path) { // Clear previous results
  memset(file_info, 0, sizeof(file_info));
  nftw(root_path, file_processor, 20, FTW_PHYS);
}


/*
*Command: w24fdb - created before or on the user specified date
*/

/*Function: Create a gzip compressed file with files created on/before user i/p date*/
void create_tar_archive_before(const char *dateString) {
    char command[2048];
    FILE *fp;

    // Construct the find command with the user-provided date, then use it to create a tar archive , used awt for date wise elimination
    snprintf(command, sizeof(command),
        "find ~/ -type f ! -path '*/.*' -exec stat --format '%%W %%n' {} + | "
        "awk -v date='%s' '"
        "{ "
        "   if ($1 != 0) { " // Check if the file has a valid birth time
        "       cmd = \"date -d @\" $1 \" '+%%Y-%%m-%%d'\"; "
        "       cmd | getline bdate; " // Get formatted birth date
        "       close(cmd); "
        "       if (bdate <= date) print $2; " // Print the file path if it meets the date criteria
        "   } "
        "}' | xargs tar -cvzf ~/temp.tar.gz 2>/dev/null", // Archive the files that match the criteria
        dateString);

    // Execute the find and tar command using popen
    fp = popen(command, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to execute command\n");
        exit(EXIT_FAILURE);
    }

    // To ensure that the command has finished executing and to capture its output
    int status = pclose(fp);
    if (status == -1) {
        fprintf(stderr, "Failed to close command stream\n");
    } else {
            printf("Archive created successfully at ~/temp.tar.gz\n", dateString);
    }
}

/*
*Command: w24fda - created after or on the user specified date
*/

/*Function: Create a gzip compressed file with files created on/after user i/p date*/
void create_tar_archive_after(const char *dateString) {
    char command[2048];
    FILE *fp;

    // Construct the find command with the user-provided date, then use it to create a tar archive , used awt for date wise elimination
    snprintf(command, sizeof(command),
        "find ~/ -type f ! -path '*/.*' -exec stat --format '%%W %%n' {} + | "
        "awk -v date='%s' '"
        "{ "
        "   if ($1 != 0) { " // Check if the file has a valid birth time
        "       cmd = \"date -d @\" $1 \" '+%%Y-%%m-%%d'\"; "
        "       cmd | getline bdate; " // Get formatted birth date
        "       close(cmd); "
        "       if (bdate >= date) print $2; " // Print the file path if it meets the date criteria (after)
        "   } "
        "}' | xargs tar -cvzf ~/temp.tar.gz 2>/dev/null", // Archive the files that match the criteria
        dateString);

    // Execute the find and tar command using popen
    fp = popen(command, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to execute command\n");
        exit(EXIT_FAILURE);
    }

    // To ensure that the command has finished executing and to capture its output
    int status = pclose(fp);
    if (status == -1) {
        fprintf(stderr, "Failed to close command stream\n");
    } else {
            printf("Archive created successfully at ~/temp.tar.gz\n", dateString);
    }
}

/*Function: Send File to client*/
void send_file(int client_socket, const char *filename) {
  //Open file created and send size/contents via socket to client
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char size_buffer[sizeof(long)];
    memcpy(size_buffer, &file_size, sizeof(long));

    // Send the size of the file to the client
    if (send(client_socket, size_buffer, sizeof(long), 0) != sizeof(long)) {
        perror("Error sending file size");
        exit(EXIT_FAILURE);
    }

    // Send the contents of the file to the client
    char buffer[1024];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) != bytes_read) {
            perror("Error sending file");
            exit(EXIT_FAILURE);
        }
    }
    printf("Bytes send by server: %zu\n",bytes_read); 
    fclose(file);
}

/*
*Command: w24fz - file size tar
*/

/*Function: Fetch files based file sizes provided and add to temp.tar.gz */
void w24fz(char *response, long size1, long size2) {
// Create the ~/w24 directory if it doesn't exist
    create_w24_directory();

  // Construct the command using snprintf
  char command[1024];
const char *homePath = getenv("HOME");
  snprintf(command, sizeof(command),
           "find %s -type f -not -path '*/.*' -size +%ldc -size -%ldc -print0 "
           "| xargs -0 tar -czf ~/w24/temp.tar.gz",
           homePath, size1, size2);

  // Open a pipe to execute the command
  FILE *fp = popen(command, "r");
  if (fp == NULL) {
    perror("popen");
    exit(EXIT_FAILURE);
  }
  // Close the pipe
  pclose(fp);
  //Response to client
  sprintf(response, "Archive created: temp.tar.gz\n");
}

/*
*Command: w24ft - file extensions based tar.gz
*/

/*Function: Fetch files based on 3 extensions(limit) provided and generate temp.tar.gz and send to client*/
void w24ft(char *response, const char *extension1, const char *extension2,
           const char *extension3) {
  // Check if at least one extension is provided
  if (extension1 == NULL && extension2 == NULL && extension3 == NULL) {
    strcpy(response, "No file type provided.\n");
    return;
  }

// Create the ~/w24 directory if it doesn't exist
    create_w24_directory();

  // Open a temporary file to store the list of filenames
  FILE *file_list = fopen("file_list.txt", "w");
  if (file_list == NULL) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }

  // Construct find command
  char command[MAX_PATH_LEN * 2]; // Double the length for safety
  sprintf(command, "find %s -type f", getenv("HOME"));

  // Append search conditions for each extension
  for (int i = 1; i <= 3; ++i) {
    const char *ext = NULL;
    switch (i) {
    case 1:
      ext = extension1;
      break;
    case 2:
      ext = extension2;
      break;
    case 3:
      ext = extension3;
      break;
    default:
      break;
    }

    if (ext != NULL) {
      sprintf(command + strlen(command), " -name '*.%s' -o", ext);
    }
  }

  // Remove the last '-o'
  if (extension1 != NULL || extension2 != NULL || extension3 != NULL) {
    command[strlen(command) - 3] = '\0';
  }

  // Append the rest of the find command with the filter for files and
  // directories starting with "."
  sprintf(command + strlen(command), " -not -path '*/\\.*'");

  // Open a pipe to execute the command
  FILE *fp = popen(command, "r");
  if (fp == NULL) {
    perror("popen");
    exit(EXIT_FAILURE);
  }

  // Read file paths from the find command output and write them to the file list
  char file_path[MAX_PATH_LEN];
  while (fgets(file_path, sizeof(file_path), fp) != NULL) {
    // Remove the newline character
    file_path[strcspn(file_path, "\n")] = '\0';
    // Write file path to file list
    fprintf(file_list, "%s\n", file_path);
  }

  // Close the find command pipe
  pclose(fp);

  // Close the file list
  fclose(file_list);

  // Open tar process using the file list
  FILE *tar_process =
      popen("tar -czf ~/w24/temp.tar.gz -T file_list.txt", "w");
  if (tar_process == NULL) {
    perror("popen");
    exit(EXIT_FAILURE);
  }

  // Close the tar process
  pclose(tar_process);

  // Check if any files were found and added to the archive
  FILE *test_tar = fopen("~/w24/temp.tar.gz", "r");
  // if (test_tar == NULL) {
  //   strcpy(response, "No files found with the specified extensions.\n");
  // } else {
   // fclose(test_tar);
    sprintf(response, "Archive created: temp.tar.gz\n");
  //}

  // Remove the temporary file list
  remove("file_list.txt");
}

/*Function: Processes all Client Commands and redirects accordingly */
void processCommands(char *tokenizer, char *response, int *valid_command,
                     int client_sock) {
  *valid_command = 1; // Assume response is valid until proven otherwise
  if (strcmp(tokenizer, "dirlist") == 0) {
    char *arg = strtok(NULL, " ");
    if (arg != NULL && strcmp(arg, "-a") == 0) {
      dirlistA(response); //response to client
    } else if (arg != NULL && strcmp(arg, "-t") == 0) {
      dirlistT(response); //response to client
    } else
      *valid_command = 0; //invalid request
  } else if (strcmp(tokenizer, "w24fn") == 0) {
    char *filename = strtok(NULL, " ");
    inputFileName = filename;
    w24fn(getenv("HOME")); //Get path of home dir
    memset(response, 0, 1048); // Clear the response buffer
    strcat(response, file_info);

    if (strlen(response) == 0) {
      sprintf(response, "File not found\n"); //If filename provided doesnot exist
    }
  } else if (strcmp(tokenizer, "w24fz") == 0) {
    memset(response, 0, 1048);
    char *size1 = strtok(NULL, " "); //fetch size 1 via tokenization
    char *size2 = strtok(NULL, " "); //fetch size2 via tokenization
    w24fz(response, atol(size1), atol(size2));
  } else if (strcmp(tokenizer, "w24ft") == 0) {
    memset(response, 0, 1048);
    char *extension1 = strtok(NULL, " "); //fetch extensions based on i/p
    char *extension2 = strtok(NULL, " ");
    char *extension3 = strtok(NULL, " ");
    printf("Extensions are : %s %s %s\n", extension1, extension2, extension3);
    if (extension1 == NULL) {
      *valid_command = 0;
    } else {
      w24ft(response, extension1, extension2, extension3);
    }
  }

  /*tar file based on creation date*/
  else if (strcmp(tokenizer, "w24fdb") == 0) {
    char *date = strtok(NULL, " ");
    memset(response, 0, 1048);
    create_tar_archive_before(date);
    char *homePath = getenv("HOME");
    // Concatenate the home directory with the rest of the file path
    char tar_filename[1024];
    snprintf(tar_filename, sizeof(tar_filename), "%s/temp.tar.gz", homePath);
    sleep(11);
    send_file(client_sock, tar_filename);
  } else if (strcmp(tokenizer, "w24fda") == 0) {
    char *date = strtok(NULL, " ");
    memset(response, 0, 1048);
    create_tar_archive_after(date);
    char *homePath = getenv("HOME");
    // Concatenate the home directory with the rest of the file path
    char tar_filename[1024];
    snprintf(tar_filename, sizeof(tar_filename), "%s/temp.tar.gz", homePath);
    // Send tar.gz file to client
    send_file(client_sock, tar_filename);
  } else {
    *valid_command = 0; //Invalid request -- No response
  }
}

/*Function: Processes client/s incoming requests based on Sec II*/
void crequest(int sock) {
  // sock - socket descriptor for client conn.
  char buffer[1024];     // store data fetched from client
  int valid_command = 1; // Validating if recieved response is correct/not
  char response[1048];   // store response response

  while (1) {
    memset(buffer, 0,
           1024); // clear buffer, prevent leftover data from previous request
    int n = read(sock, buffer, sizeof(buffer));

    if (n < 0)
      caught_error("ERROR: Issue while reading from socket");

    buffer[n] =
        '\0'; // Null-terminate the string to remove the newline character

    if (n == 0) { // Check if the client closed the connection
      printf("Client closed the connection.\n");
      break;
    }

    /* Check if client wants to QUIT */
    if (strncmp("quitc", buffer, 5) == 0) {
      sprintf(response, "Client has requested to end the session. Server "
                        "Ending session!\n");
      write(sock, response, strlen(response));
      printf("Client has ended the session.\n");
      break;
    }

    char *tokenizer = strtok(buffer, " "); // Parse CLient commands
    if (tokenizer == NULL) {
      caught_error("Error: Syntax is not valid. Please resend response.\n");
    } else {
      processCommands(tokenizer, response, &valid_command, sock);
    }

    if (valid_command) {
      write(sock, response,
            strlen(response)); // Send the processed response back to the client
    } else {
      char *error_msg = "Invalid response. Please try again!";
      write(sock, error_msg, strlen(error_msg));
    }
  }
  close(sock);
}

/*Function: Create Socket Connection: bind and connect to client*/
int setup_and_bind_socket(int portno) {
  struct sockaddr_in serv_addr;
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    caught_error("ERROR opening socket");
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY; //Any internet address can be connected
  serv_addr.sin_port = htons(portno); //PortNumber declared in global declaration

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    close(sockfd); // Ensure to close the socket on error
    caught_error("ERROR on binding");
  }

  return sockfd;
}

/*Function: Redirect to Mirror after every 3 and then alternating after 9 connections*/
void redirect_to_mirror(int client_fd, int mirror_port) {
  //printf("Mirror Port: %d, Client FD: %d\n", mirror_port, client_fd);
  char redirecting_msg[1024];
  snprintf(redirecting_msg, sizeof(redirecting_msg), "REDIRECT:%d\n",
           mirror_port);
  send(client_fd, redirecting_msg, strlen(redirecting_msg), 0);
  close(client_fd);
}

/*Function: Main - setsup the alternation logic, socket declaration and listen and acceptance of connections*/
int main(int argc, char *argv[]) {
  int sockfd, newsockfd, portno = MIRROR1_PORT;    // socket fds -  individual client connections
  socklen_t clilen;            // size of client address
  struct sockaddr_in cli_addr; // server and client address
  int pid;
  int conn_id = 1;

  // Bind socket for serverw24
  sockfd = setup_and_bind_socket(portno);
  listen(sockfd, 5);
  clilen = sizeof(cli_addr);
  printf("Mirror1 is listening on port %d...\n", portno);

  // Accept connections and handle them based on should_handle() function
  while (1) {
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,
                       &clilen); // Accept connection for serverw24
    if (newsockfd < 0)
      caught_error("ERROR: Failed while accepting connection for serverw24");

    // Mirror1 Server
      pid = fork();
      if (pid < 0)
        caught_error("ERROR: Failed while forking");
      if (pid == 0) {
        close(sockfd);
        printf("Handling connection %d\n", conn_id);
        crequest(newsockfd); // Forward commands for processing - validation
        close(newsockfd);
        exit(EXIT_SUCCESS);
      } else {
        close(newsockfd);
        conn_id++; // Increment the connection count - for alteration purposes. (Loadbalancing)
      }

  close(sockfd);
  return 0;
}
}

/*
APPENDIX:
1) Why memset and not bzero for clearing buffer?
bzero is deprecated and reduces portability

2) write() or send() to transmit comm from server - client or viceversa.

*/
