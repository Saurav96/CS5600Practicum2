#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ftw.h>
#include <sys/stat.h>
#include <time.h>
#include "../util/util.h"


int socket_desc;
struct sockaddr_in server_addr;

bool isDirectoryAvailable;
bool isRootDirectoryInit;

pthread_mutex_t root_directory_mutex;
pthread_mutex_t root_directory_availability_mutex;

void directory_changeDirectoryAvailability(bool availability)
{
  pthread_mutex_lock(&root_directory_availability_mutex);

  isDirectoryAvailable = availability;

  pthread_mutex_unlock(&root_directory_availability_mutex);
}


void directory_acquireDirectory()
{
  pthread_mutex_lock(&root_directory_mutex);

  directory_changeDirectoryAvailability(false);
}

/// @brief Releases mutex/control on Copy 1 of the server.
void directory_releaseDirectory()
{
  directory_changeDirectoryAvailability(true);

  pthread_mutex_unlock(&root_directory_mutex);
}

bool directory_isDirectoryInit()
{
  struct stat st = {0};

  if (isRootDirectoryInit)
  {
    // if directory 1 is marked as initialized, check whether it exists
    if (stat(ROOT_DIRECTORY, &st) == 0)
    {
      // it exists and is available
      printf("DIRECTORY: directory is available\n");
      return true;
    }
    else
    {
      // directory was marked initialized but isn't available anymore
      isRootDirectoryInit = false;

      printf("DIRECTORY: directorywas marked initialized but isn't available anymore\n");
      return false;
    }
  }
  return false;
}


bool directory_isDirectoryAvailable()
{
  bool res;
  if (!directory_isDirectoryInit())
  {
    printf("DIRECTORY: directory 1 is not initialized\n");
    return false;
  }
  else
  {
    pthread_mutex_lock(&root_directory_availability_mutex);

    res = isDirectoryAvailable;

    pthread_mutex_unlock(&root_directory_availability_mutex);

    if (res)
    {
      printf("AVAILABILITY: Directory is available\n");
    }
    else
    {
      printf("AVAILABILITY: Directory is not available\n");
    }

    return res;
  }
}

int init_createRootDirectory()
{
  struct stat st1 = {0};

  if (pthread_mutex_init(&root_directory_mutex, NULL) != 0)
  {
    printf("INIT ERROR: root directory 1 mutex init failed\n");
    exit(1);
  }

  if (stat(ROOT_DIRECTORY, &st1) == -1)
  {
    // created using S_IREAD, S_IWRITE, S_IEXEC (0400 | 0200 | 0100) permission flags
    int res = mkdir(ROOT_DIRECTORY, 0700);
    if (res != 0)
    {
      isRootDirectoryInit = false;
      isDirectoryAvailable = false;
      printf("INIT ERROR: root directory creation failed\n");
    }
    else
    {
      isRootDirectoryInit = true;
      isDirectoryAvailable = true;
      printf("INIT: root directory initialization successful\n");
    }
  }
  else
  {
    isRootDirectoryInit = true;
    isDirectoryAvailable = true;
    printf("INIT: root directory 1 already exists.\n");
  }
  return 0;
}

void server_closeServerSocket()
{
  close(socket_desc);
  printf("EXIT: closed server socket\n");

  exit(1);
}
void server_recieveMessageFromClient(int client_sock, char *client_message)
{
  // Receive the server's response:
  if (recv(client_sock, client_message, CODE_SIZE + CODE_PADDING + CLIENT_MESSAGE_SIZE, 0) < 0)
  {
    printf("ERROR: Error while receiving client's msg\n");
    server_closeServerSocket();
  }

  // printf("RECIEVED FROM CLIENT: %s\n", client_message);
}


void server_sendMessageToClient(int client_sock, char *server_message)
{
  // printf("SENDING TO CLIENT: %s\n", server_message);
  if (send(client_sock, server_message, strlen(server_message), 0) < 0)
  {
    printf("ERROR: Can't send\n");
    server_closeServerSocket();
  }
}




/// @brief Initializes the socket when the server goes up.
/// @return 0 if socket creation is successful, -1 otherwise.
int init_createServerSocket()
{
  // Create socket:
  socket_desc = socket(AF_INET, SOCK_STREAM, 0);

  if (socket_desc < 0)
  {
    printf("ERROR: Error while creating socket\n");
    server_closeServerSocket();
    return -1;
  }
  printf("INIT: Socket created successfully\n");
  return 0;
}

int init_bindServerSocket()
{
  // Set port and IP:
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

  // Bind to the set port and IP:S
  if (bind(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    printf("ERROR: Couldn't bind to the port\n");
    server_closeServerSocket();
    return -1;
  }
  printf("INIT: Done with binding\n");
  return 0;
}



/// @brief Listens and server for incoming client connections.
/// @return 0 if slient connection to server is successful, -1 otherwise.
int server_listenForClients()
{
  if (listen(socket_desc, 1) < 0)
  {
    printf("ERROR: Error while listening\n");
    server_closeServerSocket();
    return -1;
  }
  printf("\nListening for incoming connections.....\n");

  socklen_t client_size;
  struct sockaddr_in client_addr;

  // Accept an incoming connection:
  client_size = sizeof(client_addr);
  int client_sock = accept(socket_desc, (struct sockaddr *)&client_addr, &client_size);

  if (client_sock < 0)
  {
    printf("ERROR: Can't accept\n");
    server_closeServerSocket();
    return -1;
  }
  printf("CLIENT CONNECTION: Client connected at IP: %s and port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
  printf("CLIENT CONNECTION: Client socket: %d\n", client_sock);
  return client_sock;
}


void command_get(int client_sock, char *remote_file_path)
{
  printf("COMMAND: GET started\n");

  FILE *remote_file;

  char response_message[CODE_SIZE + CODE_PADDING + SERVER_MESSAGE_SIZE];
  memset(response_message, 0, sizeof(response_message));

  // setup available directories and respective target file paths
  char actual_path[200];
  int targetDirectory = 0;

  // wait until one of the directories becomes available
  while (true)
  {
    if (directory_isDirectoryAvailable())
    {
      directory_acquireDirectory();

      printf("GET: Directory is acquired\n");
      strcpy(actual_path, ROOT_DIRECTORY);
      targetDirectory = 1;
      break;
    }
    else
    {
      printf("GET: Waiting for available directory\n");
    }
  }

  // we have a directory available, start prep to read
  strncat(actual_path, remote_file_path, strlen(remote_file_path));

  remote_file = fopen(actual_path, "r");
  printf("GET: Looking for file: %s\n", actual_path);

  // Check if the file exists on the server
  if (remote_file == NULL)
  {
    // file doesn't exist
    printf("GET ERROR: File not found on server\n");

    strcat(response_message, "E:404 ");
    strcat(response_message, "File not found on server");

    server_sendMessageToClient(client_sock, response_message);
  }
  else
  {
    // File found on server
    printf("GET: File Found on server\n");

    // Send success response to client
    strcat(response_message, "S:200 ");
    strcat(response_message, "File found on server");

    server_sendMessageToClient(client_sock, response_message);
    memset(response_message, 0, sizeof(response_message));

    // recieve client's first response
    char client_message[CODE_SIZE + CODE_PADDING + SERVER_MESSAGE_SIZE];
    memset(client_message, '\0', sizeof(client_message));

    server_recieveMessageFromClient(client_sock, client_message);

    if (strncmp(client_message, "S:100", CODE_SIZE) == 0)
    {
      // Client said we can start sending the file
      // Send file data to client
      printf("GET: Client hinted at sending file contents.\n");
      char buffer[SERVER_MESSAGE_SIZE - 1];
      memset(buffer, 0, sizeof(buffer));
      int bytes_read;

      while (true)
      {
        if (strncmp(client_message, "S:100", CODE_SIZE) != 0)
        {
          printf("GET ERROR: stopped abruptly because client is not accepting data anymore\n");
          break;
        }

        if ((bytes_read = fread(buffer, sizeof(char), SERVER_MESSAGE_SIZE - 1, remote_file)) > 0)
        {
          // printf("BUFFER: %s \n", buffer);
          printf("GET: Continue\n");
          memset(response_message, 0, sizeof(response_message));

          strcat(response_message, "S:206 ");
          strncat(response_message, buffer, bytes_read);

          memset(buffer, 0, sizeof(buffer));

          server_sendMessageToClient(client_sock, response_message);

          memset(client_message, '\0', sizeof(client_message));

          server_recieveMessageFromClient(client_sock, client_message);
        }
        else
        {
          // Reached end of file, tell client we are done sending stuff
          printf("GET: reached end of file\n");

          memset(response_message, 0, sizeof(response_message));

          strcat(response_message, "S:200 ");
          strcat(response_message, "File sent successfully");

          server_sendMessageToClient(client_sock, response_message);
          break;
        }
      }
    }
    else
    {
      // The client is not accepting data
      memset(response_message, 0, sizeof(response_message));

      strcat(response_message, "E:500 ");
      strcat(response_message, "The client did not agree to receive the file contents.");

      server_sendMessageToClient(client_sock, response_message);

      printf("GET: The client did not agree to receive the file contents.\n");
    }
  }

  // release the directory which we are using for this command
  fclose(remote_file);
  if (targetDirectory == 1)
  {
    directory_releaseDirectory();
  }
  else
  {
    printf("GET ERROR: Unlocking directory mutex - No Directory is available\n");
  }

  printf("COMMAND: GET complete\n\n");
}

void command_write(int client_sock, char *remote_file_path)
{

    printf("COMMAND: RECEIVE started\n");
    char actual_path[200];
    char directory_path[200];
    strcpy(actual_path, ROOT_DIRECTORY);
    strncat(actual_path, remote_file_path, strlen(remote_file_path));
    printf("GET: client actual path: %s \n", actual_path);

    char directory[200];
    char file_name[200];
    sscanf(remote_file_path, "%[^/]/%s", directory, file_name);

    strcpy(directory_path, ROOT_DIRECTORY);
    strcat(directory_path, directory);
    printf("%s\n", directory_path);
  // Create the directory if it doesn't exist
    struct stat st = {0};
    if (stat(directory_path, &st) == -1) {
        mkdir(directory_path, 0700);
        printf("WRITE: Directory '%s' created\n", directory_path);
    }

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", tm_info);

    // Append the timestamp to the filename to create a new version
    char versioned_file_name[220];
    sprintf(versioned_file_name, "%s_%s", file_name, timestamp);

    // Combine the directory and versioned filename
    char versioned_actual_path[1000];
    strcpy(versioned_actual_path, ROOT_DIRECTORY);
    strcat(versioned_actual_path, directory);
    strcat(versioned_actual_path, "/");
    strcat(versioned_actual_path, versioned_file_name);
    printf("%s\n", versioned_actual_path);
    FILE *remote_file = fopen(versioned_actual_path, "w");
    if (remote_file == NULL)
    {
        printf("RECEIVE ERROR: Local file could not be opened for writing.\n");
        return;
    }

    char response_message[CODE_SIZE + CODE_PADDING + SERVER_MESSAGE_SIZE];
    memset(response_message, 0, sizeof(response_message));

    // Send success response to client to indicate server is ready to receive the file
    strcat(response_message, "S:100 ");
    strcat(response_message, "Success Continue");

    server_sendMessageToClient(client_sock, response_message);

    // Receive file data from client and write it to local file
    char client_message[CODE_SIZE + CODE_PADDING + CLIENT_MESSAGE_SIZE];
    memset(client_message, 0, sizeof(client_message));

    server_recieveMessageFromClient(client_sock, client_message);

    while (strncmp(client_message, "S:206", CODE_SIZE) == 0)
    {
        char *file_contents;
        file_contents = client_message + CODE_SIZE + CODE_PADDING;

        fwrite(file_contents, sizeof(char), strlen(file_contents), remote_file);

        // Acknowledge receipt of the block
        memset(response_message, 0, sizeof(response_message));
        strcat(response_message, "S:100 ");
        strcat(response_message, "Success Continue");

        server_sendMessageToClient(client_sock, response_message);

        // Receive the next block
        memset(client_message, 0, sizeof(client_message));
        server_recieveMessageFromClient(client_sock, client_message);
    }

    if (strncmp(client_message, "S:200", CODE_SIZE) == 0)
    {
        printf("RECEIVE: File received successfully\n");
    }
    else
    {
        printf("RECEIVE ERROR: File transfer unsuccessful\n");
    }

    fclose(remote_file);

    printf("COMMAND: RECEIVE complete\n\n");
}

bool directory_isFileExists(const char *filename)
{
  FILE *fp = fopen(filename, "r");
  bool is_exist = false;
  if (fp != NULL)
  {
    is_exist = true;
    fclose(fp); // close the file
  }
  return is_exist;
}

/// @brief Unlinks and removes a file.
/// @param fpath represents the path of file/directory.
/// @param sb buffer for stat command inside nftw command.
/// @param typeflag type for nftw command.
/// @param ftwbuf buffer for nftw command.
/// @return 0 if successful.
int directory_unlinkFile(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
  int rv = remove(fpath);

  if (rv)
    perror(fpath);

  return rv;
}

int directory_removeDirectoryRecursively(char *path)
{
  return nftw(path, directory_unlinkFile, 64, FTW_DEPTH | FTW_PHYS);
}

void command_rm(int client_sock, char *path)
{
  printf("COMMAND: RM started\n");

  // setup available directories and respective target file paths
  char actual_path1[200];

  // acquire all directories available for this command
  if (directory_isDirectoryInit())
  {
    directory_acquireDirectory();

    printf("RM: Directory is acquired\n");

    strcpy(actual_path1, ROOT_DIRECTORY);
    strncat(actual_path1, path, strlen(path));

    printf("RM: actual path: %s\n", actual_path1);
  }

 char response_message[CODE_SIZE + CODE_PADDING + SERVER_MESSAGE_SIZE];
  memset(response_message, 0, sizeof(response_message));

  struct stat sb1;

  // Check whether such a path exists in all directories
  if ((isRootDirectoryInit && stat(actual_path1, &sb1) == -1))
  {
    // Fails RM command if even one root directory doesn't have this path to remove
    printf("RM ERROR: Directory/File Not Found\n");
    perror("stat");

    strcat(response_message, "E:404 ");
    strcat(response_message, "Directory/File Not Found");

    server_sendMessageToClient(client_sock, response_message);
  }
  else
  {
    // Check whether path is a file
    if (S_ISREG(sb1.st_mode))
    {
      // Path is a regular file
      int res1 = remove(actual_path1);

      if (res1 != 0 )
      {
        // removal of file failed
        printf("RM ERROR: File Removal failed\n");
        perror("remove");
        strcat(response_message, "E:406 ");
        strcat(response_message, "File Removal failed");

        server_sendMessageToClient(client_sock, response_message);
      }
      else
      {
        // removal of file successful
        printf("RM: File Removal successful\n");

        strcat(response_message, "S:200 ");
        strcat(response_message, "File Removal successful");

        server_sendMessageToClient(client_sock, response_message);
      }
    }
    // Check whether path is a folder
    else if (S_ISDIR(sb1.st_mode))
    {
      // Path is a directory
      int res1 = directory_removeDirectoryRecursively(actual_path1);
      if (res1 != 0 )
      {
        // removal of directory failed
        printf("RM ERROR: Directory Removal failed\n");
        perror("remove");
        strcat(response_message, "E:406 ");
        strcat(response_message, "Directory Removal failed");

        server_sendMessageToClient(client_sock, response_message);
      }
      else
      {
        // removal of directory successful
        printf("RM: Directory Removal successful\n");

        strcat(response_message, "S:200 ");
        strcat(response_message, "Directory Removal successful");

        server_sendMessageToClient(client_sock, response_message);
      }
    }
    // Unsupported path
    else
    {
      printf("RM ERROR: Given path is not supported\n");
      strcat(response_message, "E:406 ");
      strcat(response_message, "Given path is not supported");

      server_sendMessageToClient(client_sock, response_message);
    }
  }

  // release the directories that were acquired for this command
  if (isRootDirectoryInit)
  {
    directory_releaseDirectory();
  }

  printf("COMMAND: RM complete\n\n");
}

#include <dirent.h>
void command_ls(int client_sock, char *remote_file_path)
{
    printf("COMMAND: LS started\n");

    char directory[200];
    char file_name[200];
    sscanf(remote_file_path, "%[^/]/%s", directory, file_name);

    // Combine the directory and filename
    char actual_path[200];
    strcpy(actual_path, ROOT_DIRECTORY);
    strcat(actual_path, directory);
  
    // Open the directory
    DIR *dir = opendir(actual_path);
    if (dir == NULL)
    {
        printf("INFO ERROR: Unable to open directory\n");
        return;
    }
    strcat(actual_path, "/");
    strcat(actual_path, file_name);

    // Initialize a structure to store file information
    struct dirent *ent;
    struct stat st;

    // Send success response to client
    char response_message[CODE_SIZE + CODE_PADDING + SERVER_MESSAGE_SIZE];
    memset(response_message, 0, sizeof(response_message));
    strcat(response_message, "S:100 ");
    strcat(response_message, "Success Continue");
    server_sendMessageToClient(client_sock, response_message);

    // Send file information to client
    while ((ent = readdir(dir)) != NULL)
    {
        char timespan[20];
        if (sscanf(ent->d_name, "%*[^_]_%19[^.]", timespan) == 1)
        {
            // Get file information
            stat(actual_path, &st);

            // Convert timestamp to a human-readable format
            char timestamp[20];
            strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", localtime(&(st.st_mtime)));
            printf("Original Timespan: %s, Formatted Timestamp: %s\n", timespan, timestamp);

            // Construct and send the file information to the client
            memset(response_message, 0, sizeof(response_message));
            strcat(response_message, "S:206 ");
            strcat(response_message, ent->d_name);
            strcat(response_message, " ");
            strcat(response_message, timespan);

            server_sendMessageToClient(client_sock, response_message);
        }
    }

    // Close the directory
    closedir(dir);

    // Send completion message to the client
    memset(response_message, 0, sizeof(response_message));
    strcat(response_message, "S:200 ");
    strcat(response_message, "File information sent successfully");
    server_sendMessageToClient(client_sock, response_message);

    printf("COMMAND: INFO complete\n\n");
}


void *server_listenForCommand(void *client_sock_arg)
{
  int client_sock = *((int *)client_sock_arg);
  free(client_sock_arg);

  char client_command[CLIENT_COMMAND_SIZE];
  memset(client_command, 0, sizeof(client_command));

  printf("LISTEN: listening for command from client socket: %d\n", client_sock);

  if (recv(client_sock, client_command, sizeof(client_command), 0) < 0)
  {
    printf("LISTEN ERROR: Couldn't listen for command\n");
    close(client_sock);
    return NULL;
  }

  char *pch;
  pch = strtok(client_command, " \n");

  char *args[3];
  args[0] = pch;

  int argcLimit;
  int argc = 1;
 
  pch = strtok(NULL, " \n");

  if (strcmp(args[0], "C:001") == 0)
  {
    argcLimit = 3;
  }
  else if(strcmp(args[0], "C:002") == 0)
  {
    argcLimit = 3;
  }
  else if (strcmp(args[0], "C:003") == 0)
  {
    argcLimit = 2;
  }
  else if (strcmp(args[0], "C:004") == 0)
  {
    argcLimit = 2;
  }
  while (pch != NULL)
  {
    if (argc >= argcLimit)
    {
      printf("LISTEN ERROR: Invalid number of arguements provided\n");
    }

    args[argc++] = pch;
    pch = strtok(NULL, " \n");
  }

  if (strcmp(args[0], "C:001") == 0)
  {
    command_write(client_sock, args[1]);
  }
  else if(strcmp(args[0], "C:002") == 0)
  {
    printf("Coming to Get\n");
    command_get(client_sock, args[1]);
  }
  else if(strcmp(args[0], "C:003") == 0)
  {
    printf("Coming to Remove");
    command_rm(client_sock, args[1]);
  }
  else if(strcmp(args[0], "C:004") == 0)
  {
    printf("Coming to Remove");
    command_ls(client_sock, args[1]);
  }
  else {
     printf("LISTEN ERROR: Invalid command provided\n");
  }

  printf("LISTEN: Closing connection for client socket %d\n", client_sock);
  close(client_sock);
  
}


int initServer()
{
  int status;
  status = init_createServerSocket();
  if (status != 0)
    return -1;
  status = init_bindServerSocket();
  if (status != 0)
    return -1;
  status = init_createRootDirectory();
  if (status != 0)
    return -1;

  return 0;
}


int main(void)
{
  int flag;

  flag = initServer();
  if (flag != 0)
    return 0;


    while (true)
  {
    // Listen for clients:
    int client_sock = server_listenForClients();
    if (client_sock < 0)
    {
      printf("CLIENT CONNECTION ERROR: Client could not be connected\n");
      continue;
    }

    // Create a new detached thread to serve client's message:
    pthread_t thread_for_client_request;
    pthread_attr_t attr;

    // detach thread so that it can end without having to join it here again
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int *arg = malloc(sizeof(*arg));
    if (arg == NULL)
    {
      fprintf(stderr, "CLIENT CONNECTION ERROR: Couldn't allocate memory for thread arguments.\n");
      exit(EXIT_FAILURE);
    }

    *arg = client_sock;

    // start listening for command from client
    pthread_create(&thread_for_client_request, NULL, server_listenForCommand, arg);
  }

  // Closing server socket:
  server_closeServerSocket();

  return 0;
}