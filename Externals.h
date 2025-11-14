// Common Header Elements common to all code

#ifndef _EXTERNALS_H_
#define _EXTERNALS_H_

#include <setjmp.h>

// Standard defines
#define LOCAL_MACHINE_IP "127.0.0.1"
#define MAX_BUFFER_SIZE 1024
#define IP_LENGTH 16
#define ERROR_MSG_LEN 100
#define INPUT_SIZE 1024
#define MAX_FILE_NAME 256

// NS IP and Port
#define NS_CLIENT_PORT 8080
#define NS_SERVER_PORT 8081
#define NS_IP LOCAL_MACHINE_IP

// Command Codes
#define CMD_READ 1
#define CMD_WRITE 2
#define CMD_CREATE 3
#define CMD_DELETE 4
#define CMD_INFO 5
#define CMD_LIST 6
#define CMD_MOVE 7
#define CMD_COPY 8
#define CMD_RENAME 9
#define CLOSE_CONNECTION 10

// Response Flags
#define RESPONSE_FLAG_SUCCESS 0
#define RESPONSE_FLAG_FAILURE -1
#define BACKUP_RESPONSE 1

// Request Flags
#define REQUEST_FLAG_SUCCESS -1
#define REQUEST_FLAG_NONE 0
#define REQUEST_FLAG_APPEND 0
#define REQUEST_FLAG_OVERWRITE 1

// ACK Flags
#define ACK_FLAG_SUCCESS 0
#define ACK_FLAG_FAILURE -1


// Request and Response Structs
/*
FLOW OF REQUEST AND RESPONSE PACKETS
Client -> Naming Server
    1. Client sends a request to the naming server
    2. Naming server sends a response to the client
Client -> Storage Server
    1. Client sends a request to the storage server
    2. Storage server sends a response to the client
Storage Server -> Naming Server
    1. Storage server sends a request to the naming server
    2. Naming server sends a response to the storage server
Naming Server -> Storage Server
    1. Naming server sends a request to the storage server
    2. Storage server sends a response to the naming server

::: Simple Pneumonic :::
    1. Receive Request
    2. Send Response
*/

// Request Struct
typedef struct REQUEST_STRUCT
{
    int iRequestOperation; // Operation to be performed
    unsigned long iRequestClientID;  // Client ID
    char sRequestPath[MAX_BUFFER_SIZE]; // Path
    int iRequestFlags;     // Flags
} REQUEST_STRUCT;

// Response Struct
typedef struct RESPONSE_STRUCT
{
    int iResponseOperation; // Operation to be performed
    int iResponseErrorCode; // Error Code
    char sResponseData[MAX_BUFFER_SIZE]; // Data
    int iResponseFlags;     // Flags
    unsigned long iResponseServerID; // Server ID
} RESPONSE_STRUCT;

// Storage-Server Init Struct
typedef struct STORAGE_SERVER_INIT_STRUCT
{
    int sServerPort_Client;  // Port on which the storage server will listen for client
    int sServerPort_NServer; // Port on which the storage server will listen for NServer

    char MountPaths[MAX_BUFFER_SIZE]; // \n separated list of mount paths
} STORAGE_SERVER_INIT_STRUCT;

// ACK Struct
typedef struct ACK_STRUCT
{
    int iAckErrorCode; // Error Code
    char sAckData[MAX_BUFFER_SIZE]; // Data
    int iAckFlags; // Flags
} ACK_STRUCT;

// Path Information Struct
typedef struct PATH_INFO_STRUCT
{
    char sPath[MAX_BUFFER_SIZE]; // Path
    int iPathType; // Type of Path
    int iPathSize; // Size of Path
    int iPathPermission; // Permission of Path
    int iPathCreationTime; // Creation Time of Path
    int iPathModificationTime; // Modification Time of Path
    int iPathAccessTime; // Access Time of Path
    int iPathLinks; // Count of Links
} PATH_INFO_STRUCT;

// // Error Catch buffer
// extern jmp_buf jmpbuffer;

int CheckError(int iStatus, char *sErrorMsg);
int CheckNull(void *ptr, char *sErrorMsg);
char* ErrorMsg(char* msg, int ErrorCode);



#endif // _EXTERNALS_H_