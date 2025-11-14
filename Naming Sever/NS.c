#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/times.h>
#include <semaphore.h>

// Local Header Files
#include "./Headers.h"
#include "./Client_Handle.h"
#include "./Server_Handle.h"
#include "./Trie.h"
#include "./LRU.h"
#include "./ErrorCodes.h"

// Global Header Files
#include "../Externals.h"
#include "../colour.h"

// Global Variables
CLIENT_HANDLE_LIST_STRUCT *clientHandleList;
SERVER_HANDLE_LIST_STRUCT *serverHandleList;
FILE *logs;
CLOCK *Clock;
TrieNode *MountTrie;
pthread_mutex_t MountTrieLock;
LRUCache *MountCache;
sem_t serverStartSem;

SERVER_HANDLE_STRUCT *ResolvePath(char *path)
{
    // Check if the path is in the cache
    SERVER_HANDLE_STRUCT *server = get(MountCache, path);
    if (server != NULL)
    {
        fprintf(logs, "[+]ResolvePath: Path %s found in cache [Time Stamp: %f]\n", path, GetCurrTime(Clock));
        return server;
    }

    // Resolve the path
    server = Get_Server(MountTrie, path);

    if (server == NULL)
    {
        fprintf(logs, "[-]ResolvePath: Path %s not found in mount trie [Time Stamp: %f]\n", path, GetCurrTime(Clock));
    }
    else
    {
        fprintf(logs, "[+]ResolvePath: Path %s found in mount trie [Time Stamp: %f]\n", path, GetCurrTime(Clock));
        // Add the path to the cache
        put(MountCache, path, server);
    }

    return server;
}

/**
 * @brief Checks if the given socket is connected( Readable )
 * @param sockfd: The socket to check
 * @return: 1 if the socket is connected, 0 if the socket is disconnected, -1 on error
 * @note: This function is non-blocking
 */
int IsSocketConnected(int sockfd)
{
    // Use recv with MSG_PEEK to check if the socket is connected
    char buff[1];
    int iRecvStatus = recv(sockfd, buff, sizeof(buff), MSG_PEEK);
    if (CheckError(iRecvStatus, "[-]IsSocketConnected: Error in receiving data from socket"))
        return -1;
    else if (iRecvStatus == 0)
        return 0;
    return 1;
}

/**
 * Initializes the clock object.
 **/
CLOCK *InitClock()
{
    CLOCK *C = (CLOCK *)malloc(sizeof(CLOCK));
    if (CheckNull(C, "[-]InitClock: Error in allocating memory"))
    {
        fprintf(logs, "[-]InitClock: Error in allocating memory\n");
        exit(EXIT_FAILURE);
    }

    C->bootTime = 0;
    C->bootTime = GetCurrTime(C);
    if (CheckError(C->bootTime, "[-]InitClock: Error in getting current time"))
    {
        fprintf(logs, "[-]InitClock: Error in getting current time\n");
        free(C);
        exit(EXIT_FAILURE);
    }

    int err = clock_gettime(CLOCK_MONOTONIC_RAW, &C->Btime);
    if (CheckError(err, "[-]InitClock: Error in getting current time"))
    {
        fprintf(logs, "[-]InitClock: Error in getting current time\n");
        free(C);
        exit(EXIT_FAILURE);
    }

    return C;
}

/**
 * Returns the current time in seconds.
 * @param Clock: The clock object.
 * @return: The current time in seconds on success, -1 on failure.
 **/
double GetCurrTime(CLOCK *Clock)
{
    if (CheckNull(Clock, "[-]GetCurrTime: Invalid clock object"))
    {
        fprintf(logs, "[-]GetCurrTime: Invalid clock object\n");
        return -1;
    }
    struct timespec time;
    int err = clock_gettime(CLOCK_MONOTONIC_RAW, &time);
    if (CheckError(err, "[-]GetCurrTime: Error in getting current time"))
    {
        fprintf(logs, "[-]GetCurrTime: Error in getting current time\n");
        return -1;
    }
    return (time.tv_sec + time.tv_nsec * 1e-9) - (Clock->bootTime);
}

void *Client_Acceptor_Thread()
{
    printf(UGRN "[+]Client Acceptor Thread Initialized\n" reset);
    fprintf(logs, "[+]Client Acceptor Thread Initialized [Time Stamp: %f]\n", GetCurrTime(Clock));

    // Create a socket
    int iServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (CheckError(iServerSocket, "[-]Client Acceptor Thread: Error in creating socket"))
        exit(EXIT_FAILURE);

    // Specify an address for the socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(NS_CLIENT_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

    // Bind the socket to our specified IP and port
    int iBindStatus = bind(iServerSocket, (struct sockaddr *)&server_address, sizeof(server_address));
    if (CheckError(iBindStatus, "[-]Client Acceptor Thread: Error in binding socket to specified IP and port"))
        exit(EXIT_FAILURE);

    // Listen for connections
    int iListenStatus = listen(iServerSocket, MAX_QUEUE_SIZE);
    if (CheckError(iListenStatus, "[-]Client Acceptor Thread: Error in listening for connections"))
        exit(EXIT_FAILURE);

    printf(GRN "[+]Client Acceptor Thread: Listening for connections\n" reset);
    fprintf(logs, "[+]Client Acceptor Thread: Listening for connections [Time Stamp: %f]\n", GetCurrTime(Clock));

    struct sockaddr_in client_address;
    socklen_t iClientSize = sizeof(client_address);
    int iClientSocket;
    // Accept a connection
    while (iClientSocket = accept(iServerSocket, (struct sockaddr *)&client_address, &iClientSize))
    {
        if (CheckError(iClientSocket, "[-]Client Acceptor Thread: Error in accepting connection"))
        {
            fprintf(logs, "[-]Client Acceptor Thread: Error in accepting connection [Time Stamp: %f]\n", GetCurrTime(Clock));
            continue;
        }

        // Store the client IP and Port in Client Handle Struct
        CLIENT_HANDLE_STRUCT clientHandle;
        strncpy(clientHandle.sClientIP, inet_ntoa(client_address.sin_addr), IP_LENGTH);
        clientHandle.sClientPort = ntohs(client_address.sin_port);
        clientHandle.iClientSocket = iClientSocket;

        // Add the client to the client list
        if (CheckError(AddClient(&clientHandle, clientHandleList), "[-]Client Acceptor Thread: Error in adding client to client list"))
        {
            fprintf(logs, "[-]Client Acceptor Thread: Error in adding client to client list [Time Stamp: %f]\n", GetCurrTime(Clock));
            close(iClientSocket);
            continue;
        }

        // Create a thread to handle the client
        pthread_t tClientHandlerThread;
        int iThreadStatus = pthread_create(&tClientHandlerThread, NULL, Client_Handler_Thread, (void *)&clientHandle);
        if (CheckError(iThreadStatus, "[-]Client Acceptor Thread: Error in creating thread"))
        {
            fprintf(logs, "[-]Client Acceptor Thread: Error in creating thread [Time Stamp: %f]\n", GetCurrTime(Clock));
            close(iClientSocket);
            continue;
        }
    }

    return NULL;
}

void *Client_Handler_Thread(void *clientHandle)
{
    CLIENT_HANDLE_STRUCT *client = (CLIENT_HANDLE_STRUCT *)clientHandle;
    printf(UGRN "[+]Client Handler Thread Initialized for Client %lu (%s:%d)\n" reset, client->ClientID, client->sClientIP, client->sClientPort);
    fprintf(logs, "[+]Client Handler Thread Initialized for Client %lu (%s:%d) [Time Stamp: %f]\n", client->ClientID, client->sClientIP, client->sClientPort, GetCurrTime(Clock));

    /*
    // Send data to the client
    char sServerResponse[MAX_BUFFER_SIZE];
    sprintf(sServerResponse, "Hello Client %lu", client->ClientID);
    int iSendStatus = send(client->iClientSocket, sServerResponse, sizeof(sServerResponse), 0);
    if(CheckError(iSendStatus, "[-]Client Handler Thread: Error in sending data to client")) return NULL;

    // Receive data from the client
    char sClientRequest[MAX_BUFFER_SIZE];
    int iRecvStatus = recv(client->iClientSocket, &sClientRequest, sizeof(sClientRequest), 0);
    if(CheckError(iRecvStatus, "[-]Client Handler Thread: Error in receiving data from client")) return NULL;

    printf(GRN"[+]Client Handler Thread: Client %lu sent: %s\n"reset, client->ClientID, sClientRequest);
    fprintf(logs, "[+]Client Handler Thread: Client %lu sent: %s\n", client->ClientID, sClientRequest);
    */

    // Send The Client It alloted ID
    unsigned long ClientID = client->ClientID;
    int iSendStatus = send(client->iClientSocket, &ClientID, sizeof(unsigned long), 0);
    if (CheckError(iSendStatus, "[-]Client Handler Thread: Error in sending data to client"))
    {
        fprintf(logs, "[-]Client Handler Thread: Error in sending data to client [Time Stamp: %f]\n", GetCurrTime(Clock));
        RemoveClient(ClientID, clientHandleList);
        close(client->iClientSocket);
        return NULL;
    }

    // Set Up request listener for the client
    int ConnStatus, CloseRequest = 0;
    while (ConnStatus = IsSocketConnected(client->iClientSocket))
    {
        // Receive the request from the client
        REQUEST_STRUCT request;

        int iRecvStatus = recv(client->iClientSocket, &request, sizeof(request), 0);
        if (CheckError(iRecvStatus, "[-]Client Handler Thread: Error in receiving data from client"))
        {
            fprintf(logs, "[-]Client Handler Thread: Error in receiving data from client [Time Stamp: %f]\n", GetCurrTime(Clock));
            RemoveClient(ClientID, clientHandleList);
            close(client->iClientSocket);
            return NULL;
        }
        else if (iRecvStatus == 0)
            break;
        // Check if the client requested to close the connection
        if (request.iRequestOperation == CLOSE_CONNECTION)
        {
            printf(UGRN "[+]Client Handler Thread: Client %lu requested to close connection\n" reset, client->ClientID);
            fprintf(logs, "[+]Client Handler Thread: Client %lu requested to close connection\n", client->ClientID);
            CloseRequest = 1;
            break;
        }
        // Handle the request (Generate a response)
        RESPONSE_STRUCT response;
        memset(&response, 0, sizeof(response));
        response.iResponseOperation = request.iRequestOperation;
        response.iResponseErrorCode = CMD_ERROR_SUCCESS;

        switch (request.iRequestOperation)
        {
        case CMD_READ:
        {
            printf(GRN "[+]Client Handler Thread: Client %lu requested to read file %s\n" reset, client->ClientID, request.sRequestPath);
            fprintf(logs, "[+]Client Handler Thread: Client %lu requested to read file %s [Time Stamp: %f]\n", client->ClientID, request.sRequestPath, GetCurrTime(Clock));
            // Do a path resolution
            SERVER_HANDLE_STRUCT *server = ResolvePath(request.sRequestPath);

            if (server == NULL)
            {
                printf(RED "[-]Client Handler Thread: Error in resolving path for client %lu\n" reset, client->ClientID);
                fprintf(logs, "[-]Client Handler Thread: Error in resolving path for client %lu [Time Stamp: %f]\n", client->ClientID, GetCurrTime(Clock));
                response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                response.iResponseErrorCode = CMD_ERROR_PATH_NOT_FOUND;
                break;
            }

            response.iResponseFlags = RESPONSE_FLAG_SUCCESS;
            // Check if the server is active
            if (IsActive(server->ServerID, serverHandleList) == 0)
            {
                // Switch to backup server
                server = GetActiveBackUp(serverHandleList, server->backupServers);
                if (server == NULL)
                {
                    fprintf(logs, "[-]Client Handler Thread: Error in getting active backup server for client %lu\n", client->ClientID);
                    response.iResponseErrorCode = CMD_ERROR_BACKUP_UNAVAILABLE;
                    response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                    break;
                }
                response.iResponseFlags = BACKUP_RESPONSE;
                fprintf(logs, "[+]Client Handler Thread: Switched to backup server %lu (%s:%d) for client %lu\n", server->ServerID, server->sServerIP, server->sServerPort_Client, client->ClientID);
            }

            printf(GRN "[+]Client Handler Thread: Resolved path %s to server %lu (%s:%d)\n" reset, request.sRequestPath, server->ServerID, server->sServerIP, server->sServerPort_Client);
            fprintf(logs, "[+]Client Handler Thread: Resolved path %s to server %lu (%s:%d)\n", request.sRequestPath, server->ServerID, server->sServerIP, server->sServerPort_Client);
            // Populate the response struct with Server IP and Port
            snprintf(response.sResponseData, MAX_BUFFER_SIZE, "%s %d", server->sServerIP, server->sServerPort_Client);
            response.iResponseServerID = server->ServerID;
            break;
        }
        case CMD_WRITE:
        {
            printf(GRN "[+]Client Handler Thread: Client %lu requested to write file %s\n" reset, client->ClientID, request.sRequestPath);
            fprintf(logs, "[+]Client Handler Thread: Client %lu requested to write file %s\n", client->ClientID, request.sRequestPath);
            // Do a path resolution
            SERVER_HANDLE_STRUCT *server = ResolvePath(request.sRequestPath);

            if (server == NULL)
            {
                printf(RED "[-]Client Handler Thread: Error in resolving path for client %lu\n" reset, client->ClientID);
                fprintf(logs, "[-]Client Handler Thread: Error in resolving path for client %lu\n", client->ClientID);
                response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                response.iResponseErrorCode = CMD_ERROR_PATH_NOT_FOUND;
                break;
            }

            response.iResponseFlags = RESPONSE_FLAG_SUCCESS;
            // Check if the server is active
            if (IsActive(server->ServerID, serverHandleList) == 0)
            {
                response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                response.iResponseErrorCode = CMD_ERROR_SERVER_UNAVAILABLE;
                break;
            }

            printf(GRN "[+]Client Handler Thread: Resolved path %s to server %lu (%s:%d)\n" reset, request.sRequestPath, server->ServerID, server->sServerIP, server->sServerPort_Client);
            fprintf(logs, "[+]Client Handler Thread: Resolved path %s to server %lu (%s:%d)\n", request.sRequestPath, server->ServerID, server->sServerIP, server->sServerPort_Client);
            // Populate the response struct with Server IP and Port
            snprintf(response.sResponseData, MAX_BUFFER_SIZE, "%s %d", server->sServerIP, server->sServerPort_Client);
            response.iResponseServerID = server->ServerID;
            break;
        }
        case CMD_INFO:
        {
            printf(GRN "[+]Client Handler Thread: Client %lu requested info for file %s\n" reset, client->ClientID, request.sRequestPath);
            fprintf(logs, "[+]Client Handler Thread: Client %lu requested info for file %s\n", client->ClientID, request.sRequestPath);

            // Do a path resolution
            SERVER_HANDLE_STRUCT *server = ResolvePath(request.sRequestPath);

            if (server == NULL)
            {
                printf(RED "[-]Client Handler Thread: Error in resolving path for client %lu\n" reset, client->ClientID);
                fprintf(logs, "[-]Client Handler Thread: Error in resolving path for client %lu\n", client->ClientID);
                response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                response.iResponseErrorCode = CMD_ERROR_PATH_NOT_FOUND;
                break;
            }

            response.iResponseFlags = RESPONSE_FLAG_SUCCESS;

            // Check if the server is active
            if (IsActive(server->ServerID, serverHandleList) == 0)
            {
                // Switch to backup server
                server = GetActiveBackUp(serverHandleList, server->backupServers);
                if (server == NULL)
                {
                    fprintf(logs, "[-]Client Handler Thread: Error in getting active backup server for client %lu\n", client->ClientID);
                    response.iResponseErrorCode = CMD_ERROR_BACKUP_UNAVAILABLE;
                    response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                    break;
                }
                response.iResponseFlags = BACKUP_RESPONSE;
                fprintf(logs, "[+]Client Handler Thread: Switched to backup server %lu (%s:%d) for client %lu\n", server->ServerID, server->sServerIP, server->sServerPort_Client, client->ClientID);
            }

            printf(GRN "[+]Client Handler Thread: Resolved path %s to server %lu (%s:%d)\n" reset, request.sRequestPath, server->ServerID, server->sServerIP, server->sServerPort_Client);
            fprintf(logs, "[+]Client Handler Thread: Resolved path %s to server %lu (%s:%d)\n", request.sRequestPath, server->ServerID, server->sServerIP, server->sServerPort_Client);

            // Populate the response struct with Server IP and Port
            snprintf(response.sResponseData, MAX_BUFFER_SIZE, "%s %d", server->sServerIP, server->sServerPort_Client);
            response.iResponseServerID = server->ServerID;

            break;
        }
        case CMD_LIST:
        {
            printf(GRN "[+]Client Handler Thread: Client %lu requested to list directory %s\n" reset, client->ClientID, request.sRequestPath);
            fprintf(logs, "[+]Client Handler Thread: Client %lu requested to list directory %s\n", client->ClientID, request.sRequestPath);

            // Populate the response struct with paths under requested path
            int err = Get_Directory_Tree(MountTrie, request.sRequestPath, response.sResponseData);
            if (err == -2)
            {
                printf(RED "[-]Client Handler Thread: Error in getting directory tree for client %lu\n" reset, client->ClientID);
                fprintf(logs, "[-]Client Handler Thread: Error in getting directory tree for client %lu\n", client->ClientID);
                response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                response.iResponseErrorCode = ERROR_GETTING_MOUNT_PATHS;
                break;
            }
            else if (err == -1)
            {
                printf(RED "[-]Client Handler Thread: Invalid Path %s for client %lu\n" reset, request.sRequestPath, client->ClientID);
                fprintf(logs, "[-]Client Handler Thread: Invalid Path %s for client %lu\n", request.sRequestPath, client->ClientID);
                response.iResponseErrorCode = CMD_ERROR_PATH_NOT_FOUND;
                response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                break;
            }

            response.iResponseFlags = RESPONSE_FLAG_SUCCESS;
            response.iResponseErrorCode = CMD_ERROR_SUCCESS;

            break;
        }

        case CMD_RENAME:
        {
            printf(GRN "[+]Client Handler Thread: Client %lu requested to rename file %s\n" reset, client->ClientID, request.sRequestPath);
            fprintf(logs, "[+]Client Handler Thread: Client %lu requested to rename file %s\n", client->ClientID, request.sRequestPath);

            char path[MAX_BUFFER_SIZE];
            strncpy(path, request.sRequestPath, MAX_BUFFER_SIZE);
            strtok(path, " ");

            // Do a path resolution
            SERVER_HANDLE_STRUCT *server = ResolvePath(request.sRequestPath);

            if (server == NULL)
            {
                printf(RED "[-]Client Handler Thread: Error in resolving path for client %lu\n" reset, client->ClientID);
                fprintf(logs, "[-]Client Handler Thread: Error in resolving path for client %lu\n", client->ClientID);
                response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                response.iResponseErrorCode = CMD_ERROR_PATH_NOT_FOUND;
                break;
            }

            response.iResponseFlags = RESPONSE_FLAG_SUCCESS;

            // Check if the server is active
            if (IsActive(server->ServerID, serverHandleList) == 0)
            {
                response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                response.iResponseErrorCode = CMD_ERROR_SERVER_UNAVAILABLE;
                break;
            }

            // Populate the response struct with Server ID
            response.iResponseServerID = server->ServerID;

            printf(GRN "[+]Client Handler Thread: Resolved path %s to server %lu (%s:%d)\n" reset, request.sRequestPath, server->ServerID, server->sServerIP, server->sServerPort_Client);
            fprintf(logs, "[+]Client Handler Thread: Resolved path %s to server %lu (%s:%d)\n", request.sRequestPath, server->ServerID, server->sServerIP, server->sServerPort_Client);

            // Forward the request to the server
            int iSendStatus = send(server->sSocket_Write, &request, sizeof(request), 0);
            if (CheckError(iSendStatus, "[-]Client Handler Thread: Error in sending request to server"))
            {
                printf(RED "[-]Client Handler Thread: Error in sending request to server for client %lu\n" reset, client->ClientID);
                fprintf(logs, "[-]Client Handler Thread: Error in sending request to server for client %lu\n", client->ClientID);
                response.iResponseFlags = RESPONSE_FLAG_FAILURE;
                response.iResponseErrorCode = CMD_ERROR_FWD_FAILED;
                break;
            }

            strncpy(response.sResponseData, "Request forwarded to server", MAX_BUFFER_SIZE);
            break;
        }

        default:
        {
            response.iResponseErrorCode = CMD_ERROR_INVALID_OPERATION;
            response.iResponseFlags = RESPONSE_FLAG_FAILURE;
            break;
        }
        }

        // Send the response to the client
        int iSendStatus = send(client->iClientSocket, &response, sizeof(response), 0);
        if (iSendStatus != sizeof(response))
        {
            printf(RED "[-]Client Handler Thread: Error in sending response to client %lu\n" reset, client->ClientID);
            fprintf(logs, "[-]Client Handler Thread: Error in sending response to client %lu\n", client->ClientID);
            break;
        }

        printf(GRN "[+]Client Handler Thread: Sent response to client %lu\n" reset, client->ClientID);
        fprintf(logs, "[+]Client Handler Thread: Sent response {%s} to client %lu\n", response.sResponseData, client->ClientID);
    }
    if (CheckError(ConnStatus, "[-]Client Handler Thread: Error in checking if socket is connected"))
    {
        printf(RED "[-]Client Handler Thread: Error in checking if socket is connected for client %lu\n" reset, client->ClientID);
        fprintf(logs, "[-]Client Handler Thread: Error in checking if socket is connected for client %lu\n", client->ClientID);
    }
    else if (CloseRequest)
    {
        printf(BHGRN "[+]Client Handler Thread: Client %lu (%s:%d) disconnected(GRACEFULLY)\n" reset, client->ClientID, client->sClientIP, client->sClientPort);
        fprintf(logs, "[+]Client Handler Thread: Client %lu (%s:%d) disconnected(GRACEFULLY)\n", client->ClientID, client->sClientIP, client->sClientPort);
    }
    else
    {
        printf(BHRED "[-]Client Handler Thread: Client %lu (%s:%d) disconnected(UNGRACEFULLY)\n" reset, client->ClientID, client->sClientIP, client->sClientPort);
        fprintf(logs, "[-]Client Handler Thread: Client %lu (%s:%d) disconnected(UNGRACEFULLY)\n", client->ClientID, client->sClientIP, client->sClientPort);
    }

    // Close the socket
    RemoveClient(ClientID, clientHandleList);
    close(client->iClientSocket);

    return NULL;
}

void *Storage_Server_Acceptor_Thread()
{
    printf(UGRN "[+]Storage Server Acceptor Thread Initialized\n" reset);
    fprintf(logs, "[+]Storage Server Acceptor Thread Initialized [Time Stamp: %f]\n", GetCurrTime(Clock));

    // Create a socket
    int iServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (CheckError(iServerSocket, "[-]Storage Server Acceptor Thread: Error in creating socket"))
        exit(EXIT_FAILURE);

    // Specify an address for the socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(NS_SERVER_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

    // Bind the socket to our specified IP and port
    int iBindStatus = bind(iServerSocket, (struct sockaddr *)&server_address, sizeof(server_address));
    if (CheckError(iBindStatus, "[-]Storage Server Acceptor Thread: Error in binding socket to specified IP and port"))
        exit(EXIT_FAILURE);

    // Listen for connections
    int iListenStatus = listen(iServerSocket, MAX_QUEUE_SIZE);
    if (CheckError(iListenStatus, "[-]Storage Server Acceptor Thread: Error in listening for connections"))
        exit(EXIT_FAILURE);

    printf(GRN "[+]Storage Server Acceptor Thread: Listening for connections\n" reset);
    fprintf(logs, "[+]Storage Server Acceptor Thread: Listening for connections [Time Stamp: %f]\n", GetCurrTime(Clock));

    struct sockaddr_in client_address;
    socklen_t iClientSize = sizeof(client_address);
    int iClientSocket;
    // Accept a connection
    while (iClientSocket = accept(iServerSocket, (struct sockaddr *)&client_address, &iClientSize))
    {
        if (CheckError(iClientSocket, "[-]Storage Server Acceptor Thread: Error in accepting connection"))
            continue;

        // Store the server IP and Port in Server Handle Struct
        SERVER_HANDLE_STRUCT serverHandle;
        strncpy(serverHandle.sServerIP, inet_ntoa(client_address.sin_addr), IP_LENGTH);
        serverHandle.sServerPort = ntohs(client_address.sin_port);
        serverHandle.sSocket_Write = iClientSocket;

        // Add the server to the server list
        if (CheckError(AddServer(&serverHandle, serverHandleList), "[-]Storage Server Acceptor Thread: Error in adding server to server list"))
        {
            close(iClientSocket);
            continue;
        }

        // Create a thread to handle the server
        pthread_t tServerHandlerThread;
        int iThreadStatus = pthread_create(&tServerHandlerThread, NULL, Storage_Server_Handler_Thread, (void *)&serverHandle);
        if (CheckError(iThreadStatus, "[-]Storage Server Acceptor Thread: Error in creating thread"))
            continue;
    }
}

void *Storage_Server_Handler_Thread(void *storageServerHandle)
{
    SERVER_HANDLE_STRUCT *server = (SERVER_HANDLE_STRUCT *)storageServerHandle;
    printf(UGRN "[+]Storage Server Handler Thread Initialized for Server (%s:%d)\n" reset, server->sServerIP, server->sServerPort);
    fprintf(logs, "[+]Storage Server Handler Thread Initialized for Server (%s:%d) [Time Stamp: %f]\n", server->sServerIP, server->sServerPort, GetCurrTime(Clock));

    // Post the semaphore to indicate that a server is online
    sem_post(&serverStartSem);

    // Check if enough servers are running for backups
    if (serverHandleList->iServerCount < (BACKUP_SERVERS + 1))
    {
        printf(YELHB "[+]Storage Server Handler Thread: Waiting for enough servers to be online\n" reset);
        fprintf(logs, "[+]Storage Server Handler Thread: Waiting for other servers to start [Time Stamp: %f]\n", GetCurrTime(Clock));

        sem_wait(&serverStartSem);

        printf(GRN "[+]Storage Server Handler Thread: servers online\n" reset);
        fprintf(logs, "[+]Storage Server Handler Thread: servers online [Time Stamp: %f]\n", GetCurrTime(Clock));
    }

    // Recieve the Server Init Packet
    STORAGE_SERVER_INIT_STRUCT serverInitPacket;
    int iRecvStatus = recv(server->sSocket_Write, &serverInitPacket, sizeof(serverInitPacket), 0);
    if (CheckError(iRecvStatus, "[-]Storage Server Handler Thread: Error in receiving data from server"))
    {
        RemoveServer(GetServerID(server), serverHandleList);
        close(server->sSocket_Write);
        return NULL;
    }

    // Unpack the Server Init Packet
    server->sServerPort_Client = serverInitPacket.sServerPort_Client;
    server->sServerPort_NServer = serverInitPacket.sServerPort_NServer;

    // Extract indivisual path from the mount paths string (tokenize on \n) and Insert into the mount trie
    char *token = serverInitPacket.MountPaths;
    while (strlen(token))
    {
        char *path_tok = __strtok_r(token, "\n", &token);
        // Removing the the first token in the path [e.g. (server name/~) , (./~) , (mount/~) , etc.]
        // Is handled by the Insert_Path function
        int err_code = Insert_Path(MountTrie, path_tok, server);
        if (CheckError(err_code, "[-]Storage Server Handler Thread: Error in inserting path into mount trie"))
        {
            fprintf(logs, "[-]Storage Server Handler Thread: Error in inserting path into mount trie\n");
            RemoveServer(GetServerID(server), serverHandleList);
            close(server->sSocket_Write);
            return NULL;
        }
    }

    printf(GRN "[+]Storage Server Handler Thread: Server %lu (%s:%d) Paths Inserted\n" reset, server->ServerID, server->sServerIP, server->sServerPort);
    fprintf(logs, "[+]Storage Server Handler Thread: Server %lu (%s:%d) Paths Inserted [Time Stamp: %f]\n", server->ServerID, server->sServerIP, server->sServerPort, GetCurrTime(Clock));

    printf(BHWHT "{Current Mount Trie}\n" reset);
    Print_Trie(MountTrie, 0);

    // Set Up the Backup Servers for the server
    int err_code = AssignBackupServer(serverHandleList, server->ServerID);
    if (CheckError(err_code, "[-]Storage Server Handler Thread: Error in assigning backup servers"))
    {
        RemoveServer(server->ServerID, serverHandleList);
        close(server->sSocket_Write);
        return NULL;
    }

    // Set Up Backups in the backup servers (handle later)

    // Send the server ID to the server
    unsigned long ServerID = server->ServerID;
    int iSendStatus = send(server->sSocket_Write, &ServerID, sizeof(unsigned long), 0);
    if (CheckError(iSendStatus, "[-]Storage Server Handler Thread: Error in sending ID to server"))
    {
        fprintf(logs, "[-]Storage Server Handler Thread: Error in sending data to server [Time Stamp: %f]\n", GetCurrTime(Clock));
        RemoveServer(GetServerID(server), serverHandleList);
        close(server->sSocket_Write);
        return NULL;
    }

    // SetUp listner for the server
    int iServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (CheckError(iServerSocket, "[-]Storage Server Handler Thread: Error in creating socket"))
    {
        RemoveServer(GetServerID(server), serverHandleList);
        close(server->sSocket_Write);
        return NULL;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server->sServerPort_NServer);
    server_address.sin_addr.s_addr = inet_addr(server->sServerIP);
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

    int iconnectStatus = -1;
    int tries = 0;
    while (iconnectStatus < 0)
    {
        iconnectStatus = connect(iServerSocket, (struct sockaddr *)&server_address, sizeof(server_address));
        if (CheckError(iconnectStatus, "[-]Storage Server Handler Thread: Error in connecting to server."))
        {
            if (tries > MAX_CONN_REQ)
            {
                printf(RED "[-]Storage Server Handler Thread: Error in connecting to server. Max tries reached\n" reset);
                fprintf(logs, "[-]Storage Server Handler Thread: Error in connecting to server. Max tries reached [Time Stamp: %f]\n", GetCurrTime(Clock));
                RemoveServer(GetServerID(server), serverHandleList);
                close(server->sSocket_Write);
                return NULL;
            }
            printf("Trying Again...\n");
            fprintf(logs, "[-]Storage Server Handler Thread: Error in connecting to server.Trying Again... [Time Stamp: %f]\n", GetCurrTime(Clock));
            tries++;
            sleep(CONN_TIMEOUT);
        }
    }

    printf(GRN "[+]Storage Server Handler Thread: Connected to server %lu (%s:%d) for listening\n" reset, server->ServerID, server->sServerIP, server->sServerPort_NServer);
    fprintf(logs, "[+]Storage Server Handler Thread: Connected to server %lu (%s:%d) for listening [Time Stamp: %f]\n", server->ServerID, server->sServerIP, server->sServerPort_NServer, GetCurrTime(Clock));

    server->sSocket_Read = iServerSocket;

    while (1)
    {
        // Receive the response from the server
        RESPONSE_STRUCT response_struct;
        RESPONSE_STRUCT *response = &response_struct;
        int iRecvStatus = recv(server->sSocket_Write, response, sizeof(response_struct), 0);
        if (CheckError(iRecvStatus, "[-]Storage Server Handler Thread: Error in receiving data from server"))
        {
            RemoveServer(GetServerID(server), serverHandleList);
            close(server->sSocket_Write);
            return NULL;
        }
        else if (iRecvStatus == 0)
        {
            printf(RED "[-]Storage Server Handler Thread: Server %lu (%s:%d) disconnected(UNGRACEFULLY)\n" reset, server->ServerID, server->sServerIP, server->sServerPort);
            fprintf(logs, "[-]Storage Server Handler Thread: Server %lu (%s:%d) disconnected(UNGRACEFULLY) [Time Stamp: %f]\n", server->ServerID, server->sServerIP, server->sServerPort, GetCurrTime(Clock));

            close(server->sSocket_Write);
            close(server->sSocket_Read);
            int err_code = SetInactive(server->ServerID, serverHandleList);
            if (CheckError(err_code, "[-]Storage Server Handler Thread: Error in setting server inactive"))
            {
                RemoveServer(GetServerID(server), serverHandleList);
                close(server->sSocket_Write);
            }

            return NULL;
        }

        // Handle the request (Forward the response to respective client/server)
        printf(GRN "[+]Storage Server Handler Thread: Request received from server %lu\n" reset, server->ServerID);
        fprintf(logs, "[+]Storage Server Handler Thread: Request received from server %lu [Time Stamp: %f]\n", server->ServerID, GetCurrTime(Clock));

        switch (response->iResponseOperation)
        {
        case CMD_RENAME:
        {
            ACK_STRUCT ack_struct;
            ACK_STRUCT *ack = &ack_struct;

            unsigned long clientID;

            char data[MAX_BUFFER_SIZE];
            char *token;
            strncpy(data, response->sResponseData, MAX_BUFFER_SIZE);
            __strtok_r(data, " ", &token);

            clientID = (unsigned long)(atoll(token));

            ack->iAckErrorCode = response->iResponseErrorCode;
            strncpy(ack->sAckData, data, MAX_BUFFER_SIZE);
            ack->iAckFlags = response->iResponseFlags;

            if (response->iResponseErrorCode)
            {
                printf(RED "[-]Storage Server Handler Thread: Error in renaming file\n" reset);
                fprintf(logs, "[-]Storage Server Handler Thread: Error in renaming file [Time Stamp: %f]\n", GetCurrTime(Clock));
            }
            else
            {
                printf(GRN "[+]Storage Server Handler Thread: File renamed successfully\n" reset);
                fprintf(logs, "[+]Storage Server Handler Thread: File renamed successfully [Time Stamp: %f]\n", GetCurrTime(Clock));
            }

            // forward to corresponding client
            // find the client
            CLIENT_HANDLE_STRUCT *client = GetClient(clientID, clientHandleList);
            if (client == NULL)
            {
                printf(RED "[-]Storage Server Handler Thread: Error in finding client\n" reset);
                fprintf(logs, "[-]Storage Server Handler Thread: Error in finding client [Time Stamp: %f]\n", GetCurrTime(Clock));
                break;
            }

            int iSendStatus = send(client->iClientSocket, ack, sizeof(ack_struct), 0);
            if (CheckError(iSendStatus, "[-]Storage Server Handler Thread: Error in sending data to client"))
            {
                fprintf(logs, "[-]Storage Server Handler Thread: Error in sending data to client [Time Stamp: %f]\n", GetCurrTime(Clock));
                RemoveClient(clientID, clientHandleList);
                close(client->iClientSocket);
                break;
            }

            printf(GRN "[+]Storage Server Handler Thread: Sent ack to client %lu\n" reset, clientID);
            fprintf(logs, "[+]Storage Server Handler Thread: Sent ack to client %lu [Time Stamp: %f]\n", clientID, GetCurrTime(Clock));
            break;
        }
        }
    }

    // Disconnect gracefully
    printf(URED "[-]Storage Server Handler Thread: Server %lu (%s:%d) disconnected(GRACEFULLY)\n" reset, server->ServerID, server->sServerIP, server->sServerPort);
    fprintf(logs, "[-]Storage Server Handler Thread: Server %lu (%s:%d) disconnected(GRACEFULLY) [Time Stamp: %f]\n", server->ServerID, server->sServerIP, server->sServerPort, GetCurrTime(Clock));
    close(server->sSocket_Write);
    close(server->sSocket_Read);
    RemoveServer(GetServerID(server), serverHandleList);
    return NULL;
}

void *Log_Flusher_Thread()
{
    while (1)
    {
        sleep(LOG_FLUSH_INTERVAL);
        printf(BBLK "[+]Log Flusher Thread: Flushing logs\n" reset);

        fprintf(logs, "[+]Log Flusher Thread: Flushing logs [Time Stamp: %f]\n", GetCurrTime(Clock));
        fprintf(logs, "------------------------------------------------------------\n");
        fprintf(logs, "Current Mount Trie:\n");
        char buffer[MAX_BUFFER_SIZE];
        memset(buffer, 0, MAX_BUFFER_SIZE);
        int err = Get_Directory_Tree(MountTrie, "/", buffer);
        if (CheckError(err, "[-]Log_Flusher_Thread: Error in getting directory tree"))
        {
            fprintf(logs, "[-]Log_Flusher_Thread: Error in getting directory tree\n");
            exit(EXIT_FAILURE);
        }
        fprintf(logs, "%s\n", buffer);
        fprintf(logs, "Number of Current Clients: %d\n", clientHandleList->iClientCount);
        fprintf(logs, "Number of Current Servers: %d\n", serverHandleList->iServerCount);
        fprintf(logs, "------------------------------------------------------------\n");

        fflush(logs);
    }
    return NULL;
}

void exit_handler()
{
    printf(BRED "[-]Server Exiting\n" reset);
    fprintf(logs, "[-]Server Exiting [Time Stamp: %f]\n", GetCurrTime(Clock));
    for (int i = 0; i < clientHandleList->iClientCount; i++)
    {
        close(clientHandleList->clientList[i].iClientSocket);
    }
    for (int i = 0; i < serverHandleList->iServerCount; i++)
    {
        close(serverHandleList->serverList[i].sSocket_Read);
        close(serverHandleList->serverList[i].sSocket_Write);
    }
    Delete_Trie(MountTrie);
    pthread_mutex_destroy(&MountTrieLock);
    freeCache(MountCache);
    fclose(logs);
}

int main(int argc, char *argv[])
{
    // Open the logs file
    logs = fopen("NSlog.log", "w");

    // Register the exit handler
    atexit(exit_handler);

    // Initialize the Naming Server Global Variables
    clientHandleList = InitializeClientHandleList();
    serverHandleList = InitializeServerHandleList();
    sem_init(&serverStartSem, 0, -BACKUP_SERVERS);

    // Initialize the Mount Paths Trie
    MountTrie = Init_Trie();
    pthread_mutex_init(&MountTrieLock, NULL);
    strcpy(MountTrie->path_token, "Mount");
    MountTrie->Server_Handle = NULL;

    // Initialize the LRU Cache
    MountCache = createCache();

    // Initialize the clock object
    Clock = InitClock();

    // Create a thread to flush the logs periodically
    pthread_t tLogFlusherThread;
    int iThreadStatus = pthread_create(&tLogFlusherThread, NULL, Log_Flusher_Thread, NULL);
    if (CheckError(iThreadStatus, "[-]Error in creating thread"))
        return 1;

    printf(BGRN "[+]Naming Server Initialized\n" reset);
    fprintf(logs, "[+]Naming Server Initialized [Time Stamp: %f]\n", GetCurrTime(Clock));

    // Create a thread to accept client connections
    pthread_t tClientAcceptorThread;
    iThreadStatus = pthread_create(&tClientAcceptorThread, NULL, Client_Acceptor_Thread, NULL);
    if (CheckError(iThreadStatus, "[-]Error in creating thread"))
        return 1;

    // Create a thread to accept storage server connections
    pthread_t tStorageServerAcceptorThread;
    iThreadStatus = pthread_create(&tStorageServerAcceptorThread, NULL, Storage_Server_Acceptor_Thread, NULL);
    if (CheckError(iThreadStatus, "[-]Error in creating thread"))
        return 1;

    // Wait for the thread to terminate
    pthread_join(tClientAcceptorThread, NULL);
    pthread_join(tStorageServerAcceptorThread, NULL);

    return 0;
}