#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include "../util/util.h"
 
int socket_desc;
struct sockaddr_in server_addr;


bool isDirectoryAvailable;
bool isRootDirectoryInit;

pthread_mutex_t client_directory_mutex;
pthread_mutex_t client_directory_availability_mutex;

void directory_changeDirectoryAvailability(bool availability)
{
  pthread_mutex_lock(&client_directory_availability_mutex);

  isDirectoryAvailable = availability;

  pthread_mutex_unlock(&client_directory_availability_mutex);
}


void directory_acquireDirectory()
{
  pthread_mutex_lock(&client_directory_mutex);

  directory_changeDirectoryAvailability(false);
}

/// @brief Releases mutex/control on Copy 1 of the server.
void directory_releaseDirectory()
{
  directory_changeDirectoryAvailability(true);

  pthread_mutex_unlock(&client_directory_mutex);
}

bool directory_isDirectoryInit()
{
  struct stat st = {0};

  if (isRootDirectoryInit)
  {
    // if directory 1 is marked as initialized, check whether it exists
    if (stat(CLIENT_DIRECTORY, &st) == 0)
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
    pthread_mutex_lock(&client_directory_availability_mutex);

    res = isDirectoryAvailable;

    pthread_mutex_unlock(&client_directory_availability_mutex);

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

  if (pthread_mutex_init(&client_directory_mutex, NULL) != 0)
  {
    printf("INIT ERROR: root directory 1 mutex init failed\n");
    exit(1);
  }

  if (stat(CLIENT_DIRECTORY, &st1) == -1)
  {
    // created using S_IREAD, S_IWRITE, S_IEXEC (0400 | 0200 | 0100) permission flags
    int res = mkdir(CLIENT_DIRECTORY, 0700);
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


void client_closeClientSocket()
{
  // Close the socket:
  close(socket_desc);
  printf("EXIT: closing client socket\n");
  exit(1);
}

void client_sendMessageToServer(char client_message[CODE_SIZE + CODE_PADDING + CLIENT_MESSAGE_SIZE])
{
  printf("SENDING TO SERVER: %s \n", client_message);

  // Send the message to server:
  if (send(socket_desc, client_message, strlen(client_message), 0) < 0)
  {
    printf("ERROR: Unable to send message \n");
    client_closeClientSocket();
  }
}

void client_recieveMessageFromServer(char *server_message)
{
  // Receive the server's response:
  if (recv(socket_desc, server_message, CODE_SIZE + CODE_PADDING + SERVER_MESSAGE_SIZE, 0) < 0)
  {
    printf("ERROR: Error while receiving server's message \n");
    client_closeClientSocket();
  }

  printf("RECIEVED FROM SERVER: %s \n", server_message);
}


void client_connect()
{
  // Set port and IP the same as server-side:
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

  // Send connection request to server:
  if (connect(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    printf("ERROR: Unable to connect\n");
    client_closeClientSocket();
  }
  printf("Connected with server successfully\n");
}

void command_get(char *remote_file_path, char *local_file_path)
{
  printf("COMMAND: GET started\n");

  // Open local file for writing
  FILE *local_file;
  char actual_path[200];
  strcpy(actual_path, CLIENT_DIRECTORY);
  strncat(actual_path, local_file_path, strlen(local_file_path));
  printf("GET: client actual path: %s \n", actual_path);
  
  char directory[200];
  char file_name[200];
  sscanf(remote_file_path, "%[^/]/%s", directory, file_name);

  // Create the directory if it doesn't exist
    struct stat st = {0};
    if (stat(directory, &st) == -1) {
        mkdir(directory, 0700);
        printf("WRITE: Directory '%s' created\n", directory);
    }

  local_file = fopen(actual_path, "w");

  if (local_file == NULL)
  {
    printf("GET ERROR: Local file could not be opened. Please check whether the location exists.\n");
  }
  else
  {
    // Connect to server socket:
    client_connect();

    char client_message[CODE_SIZE + CODE_PADDING + CLIENT_MESSAGE_SIZE];
    memset(client_message, 0, sizeof(client_message));
    char server_response[CODE_SIZE + CODE_PADDING + SERVER_MESSAGE_SIZE];
    memset(server_response, 0, sizeof(server_response));

    // sending message to server
    char code[CODE_SIZE + CODE_PADDING] = "C:002 ";
    strncat(client_message, code, CODE_SIZE + CODE_PADDING);

    strncat(client_message, remote_file_path, strlen(remote_file_path));
    strncat(client_message, " ", 1);
    strncat(client_message, local_file_path, strlen(local_file_path));

    client_sendMessageToServer(client_message);

    // Receive server response
    client_recieveMessageFromServer(server_response);

    // Check if the file exists on the server
    if (strncmp(server_response, "S:200", CODE_SIZE) == 0)
    {
      printf("GET: File Found - Server Response: %s \n", server_response);

      // Hint the server to send the file data requested
      memset(client_message, 0, sizeof(client_message));
      strcat(client_message, "S:100 ");
      strcat(client_message, "Success Continue");

      client_sendMessageToServer(client_message);

      // Receive file data from server and write it to local file
      // get first block from server
      memset(server_response, 0, sizeof(server_response));
      client_recieveMessageFromServer(server_response);

      // continue taking blocks from server until it is done
      while (true)
      {
        if (strncmp(server_response, "S:206", CODE_SIZE) == 0)
        {
          char *file_contents;
          file_contents = server_response + CODE_SIZE + CODE_PADDING;

          // printf("GET: Writing to file: %s\n", file_contents);

          fwrite(file_contents, sizeof(char), strlen(file_contents), local_file);

          memset(client_message, 0, sizeof(client_message));
          strcat(client_message, "S:100 ");
          strcat(client_message, "Success Continue");

          client_sendMessageToServer(client_message);

          // get next block from server
          memset(server_response, 0, sizeof(server_response));
          client_recieveMessageFromServer(server_response);
        }
        else if (strncmp(server_response, "E:500", CODE_SIZE) == 0)
        {
          printf("GET ERROR: File could not be recieved\n");

          break;
        }
        else if (strncmp(server_response, "S:200", CODE_SIZE) == 0)
        {
          printf("GET: File received successfully\n");

          break;
        }
      }

      fclose(local_file);
    }
    else
    {
      printf("GET: File Not Found - Server Response: %s \n", server_response);
    }
  }

  printf("COMMAND: GET complete\n\n");
}

void command_write(char *local_file_path, char *remote_file_path)
{
    char actual_path[200];
    int targetDirectory = 0;

    while (true)
    {
        if (directory_isDirectoryAvailable())
        {
            directory_acquireDirectory();
            printf("GET: Directory is acquired\n");
            targetDirectory = 1;
            strcpy(actual_path, CLIENT_DIRECTORY);
            break;
        }
        else
        {
            printf("GET: Waiting for available directory\n");
        }
    }

    strncat(actual_path, local_file_path, strlen(local_file_path));
    printf("GET: client actual path: %s \n", actual_path);
    printf("COMMAND: SEND started\n");

    // Open local file for reading
    FILE *local_file = fopen(local_file_path, "r");
    if (local_file == NULL)
    {
        printf("SEND ERROR: Local file could not be opened. Please check whether the file exists.\n");
        return;
    }

    // Connect to server socket
    client_connect();

    char client_message[CODE_SIZE + CODE_PADDING + CLIENT_MESSAGE_SIZE];
    memset(client_message, 0, sizeof(client_message));
    char server_response[CODE_SIZE + CODE_PADDING + SERVER_MESSAGE_SIZE];
    memset(server_response, 0, sizeof(server_response));

    // Sending message to server
    char code[CODE_SIZE + CODE_PADDING] = "C:001 ";
    strncat(client_message, code, CODE_SIZE + CODE_PADDING);

    strncat(client_message, remote_file_path, strlen(remote_file_path));
    strncat(client_message, " ", 1);
    strncat(client_message, local_file_path, strlen(local_file_path));

    client_sendMessageToServer(client_message);

    // Receive server response
    client_recieveMessageFromServer(server_response);

    // Check if the server is ready to receive the file
    if (strncmp(server_response, "S:100", CODE_SIZE) == 0)
    {
        // Send file data to server
        char buffer[SERVER_MESSAGE_SIZE - 1];
        memset(buffer, 0, sizeof(buffer));
        int bytes_read;

        while ((bytes_read = fread(buffer, sizeof(char), SERVER_MESSAGE_SIZE - 1, local_file)) > 0)
        {
            memset(client_message, 0, sizeof(client_message));
            strcat(client_message, "S:206 ");
            strncat(client_message, buffer, bytes_read);

            client_sendMessageToServer(client_message);

            memset(server_response, 0, sizeof(server_response));
            client_recieveMessageFromServer(server_response);

            if (strncmp(server_response, "S:100", CODE_SIZE) != 0)
            {
                printf("SEND ERROR: Stopped abruptly because the server is not accepting data anymore\n");
                break;
            }

            memset(buffer, 0, sizeof(buffer));
        }

        // Notify the server that file sending is complete
        memset(client_message, 0, sizeof(client_message));
        strcat(client_message, "S:200 ");
        strcat(client_message, "File sent successfully");

        client_sendMessageToServer(client_message);

        printf("SEND: File sent successfully\n");
    }
    else
    {
        printf("SEND ERROR: Server did not agree to receive the file contents.\n");
    }

    fclose(local_file);
    if (targetDirectory == 1)
    {
        directory_releaseDirectory();
    }
    else
    {
        printf("GET ERROR: Unlocking directory mutex - No Directory is available\n");
    }
    printf("COMMAND: SEND complete\n\n");
}


void command_remove(char *path)
{
  printf("COMMAND: RM started\n");

  char client_message[CODE_SIZE + CODE_PADDING + CLIENT_MESSAGE_SIZE];
  char server_message[CODE_SIZE + CODE_PADDING + SERVER_MESSAGE_SIZE];

  // empty string init
  memset(client_message, '\0', sizeof(client_message));
  memset(server_message, '\0', sizeof(server_message));

  // build command to send to server
  char code[CODE_SIZE + CODE_PADDING] = "C:003 ";
  strncat(client_message, code, CODE_SIZE + CODE_PADDING);
  strncat(client_message, path, strlen(path));

  // Connect to server socket:
  client_connect();

  // send command to server
  client_sendMessageToServer(client_message);

  // get response from server
  client_recieveMessageFromServer(server_message);

  memset(server_message, 0, sizeof(server_message));

  printf("COMMAND: RM complete\n\n");
}

void client_parseCommand(int argsCount, char **argv)
{
     if (strcmp(argv[1], "WRITE") == 0)
    {
        if (argsCount == 3)
        {
            command_write(argv[2], argv[2]);
        }
        else if (argsCount == 4)
        {
            command_write(argv[2], argv[3]);
        }
        else
        {
            printf("ERROR: Invalid number of arguements provided\n");
        }
    }
   else if (strcmp(argv[1], "GET") == 0)
    {
        if (argsCount == 3)
        {
            command_get(argv[2], argv[2]);
        }
        else if (argsCount == 4)
        {
            command_get(argv[2], argv[3]);
        }
    }
    else if (strcmp(argv[1], "RM") == 0)
    {
        if (argsCount == 3)
        {
            if (argv[2][0] == '.' || argv[2][0] == '/')
            {
                printf("ERROR: Invalid arguements provided\n");
             }
            printf("%s", argv[2]);
            command_remove(argv[2]);
        }
    }
    else
    {
        printf("ERROR: Invalid number of arguements provided\n");
    }
}
void init_createClientSocket()
{
  // Create socket:
  socket_desc = socket(AF_INET, SOCK_STREAM, 0);

  if (socket_desc < 0)
  {
    printf("ERROR: Unable to create socket\n");
    client_closeClientSocket();
  }

  printf("INIT: Socket created successfully\n");
}

int main(int argc, char **argv)
{
  if (argc > 4)
  {
    printf("Incorrect number of arguements supplied\n");
    return 0;
  }

  if (strcmp(argv[1], "WRITE") != 0 &&
      strcmp(argv[1], "GET") != 0 &&
      strcmp(argv[1], "RM") != 0)
  {
    printf("Incorrect command provided!: %s\n", argv[1]);
    return 0;
  }

  // Initialize client socket:
  init_createClientSocket();
  int status = init_createRootDirectory();
  // Get text message to send to server:
  client_parseCommand(argc, argv);

  // Closing client socket:
  client_closeClientSocket();

  return 0;
}