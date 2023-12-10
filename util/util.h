#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <sys/time.h>
#include <pthread.h>

#define CODE_SIZE 5
#define CODE_PADDING 1

// Error codes
#define NOT_FOUND "E:404"
#define NOT_ACCEPTABLE "E:406"

// Success codes
#define SUCCESS_OK "S:200"
#define SUCCESS_CONTINUE "S:100"
#define SUCCESS_PARTIAL_CONTENT "S:206"

#define COMMAND_CODE_WRITE "C:001"
#define COMMAND_CODE_GET "C:002"

#define SERVER_MESSAGE_SIZE 2000
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 2000

#define CLIENT_MESSAGE_SIZE 2000
#define CLIENT_COMMAND_SIZE 1000
#define ROOT_DIRECTORY "/home/sashaw/Downloads/CS5600Practicum2/server/server_client/"
#define CLIENT_DIRECTORY "/home/sashaw/Downloads/CS5600Practicum2/client/"

#endif 


