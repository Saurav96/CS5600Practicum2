#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ftw.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include "../util/util.h"


int socket_desc;
struct sockaddr_in server_addr;

bool isDirectory1Available;
bool isDirectory2Available;


void server_closeServerSocket()
{
  close(socket_desc);
  printf("EXIT: closed server socket\n");

  exit(1);
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

int init_createRootDirectory()
{
  struct stat st1 = {0};
  struct stat st2 = {0};

  bool isDirectory1Fresh = false;
  bool isDirectory1Available = false;
  bool isRootDirectory1Init = false;

  pthread_mutex_t root_directory_1_mutex;


  // init mutex for directory 1
  if (pthread_mutex_init(&root_directory_1_mutex, NULL) != 0)
  {
    printf("INIT ERROR: root directory 1 mutex init failed\n");
    exit(1);
  }

  // init directory 1
  if (stat(ROOT_DIRECTORY, &st1) == -1)
  {
    // created using S_IREAD, S_IWRITE, S_IEXEC (0400 | 0200 | 0100) permission flags
    int res = mkdir(ROOT_DIRECTORY, 0700);
    if (res != 0)
    {
      isRootDirectory1Init = false;
      isDirectory1Available = false;
      printf("INIT ERROR: root directory 1 creation failed\n");
    }
    else
    {
      isRootDirectory1Init = true;
      isDirectory1Available = true;
      isDirectory1Fresh = true;
      printf("INIT: root directory 1 initialization successful\n");
    }
  }
  else
  {
    isRootDirectory1Init = true;
    isDirectory1Available = true;
    isDirectory1Fresh = false;
    printf("INIT: root directory 1 already exists.\n");
  }
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
  else {
     printf("LISTEN ERROR: Invalid command provided\n");
  }

  printf("LISTEN: Closing connection for client socket %d\n", client_sock);
  close(client_sock);

}

void command_write(char *local_file_path, char *remote_file_path)
{
  printf("COMMAND: GET started\n");

  // Open local file for writing
  FILE *remote_file;
  char actual_path[200];
  strcpy(actual_path, ROOT_DIRECTORY);
  strncat(actual_path, remote_file_path, strlen(remote_file_path));
  printf("GET: actual path: %s \n", actual_path);

  remote_file = fopen(actual_path, "w");

  if (remote_file == NULL)
  {
    printf("GET ERROR: Local file could not be opened. Please check whether the location exists.\n");
  }


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