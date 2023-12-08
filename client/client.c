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

  if (strcmp(argv[1], "WRITE") != 0) 
  {
    printf("Incorrect command provided!: %s\n", argv[1]);
    return 0;
  }

  // Initialize client socket:
  init_createClientSocket();

  // Get text message to send to server:
  client_parseCommand(argc, argv);

  // Closing client socket:
  client_closeClientSocket();

  return 0;
}