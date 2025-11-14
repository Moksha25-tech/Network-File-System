#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>

#include "./Headers.h"
#include "./Trie.h"
#include "./ErrorCodes.h"
#include "../Externals.h"
#include "../colour.h"

int NS_Write_Socket;
Trie *File_Trie;
unsigned long Server_ID;

FILE *Log_File;
CLOCK *Clock;

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
 * @brief Initializes the clock object.
 * @return: A pointer to the clock object on success, NULL on failure.
 **/
CLOCK *InitClock()
{
    CLOCK *C = (CLOCK *)malloc(sizeof(CLOCK));
    if (CheckNull(C, "[-]InitClock: Error in allocating memory"))
    {
        fprintf(Log_File, "[-]InitClock: Error in allocating memory \n");
        exit(EXIT_FAILURE);
    }

    C->bootTime = 0;
    C->bootTime = GetCurrTime(C);
    if (CheckError(C->bootTime, "[-]InitClock: Error in getting current time"))
    {
        fprintf(Log_File, "[-]InitClock: Error in getting current time\n");
        free(C);
        exit(EXIT_FAILURE);
    }

    int err = clock_gettime(CLOCK_MONOTONIC_RAW, &C->Btime);
    if (CheckError(err, "[-]InitClock: Error in getting current time"))
    {
        fprintf(Log_File, "[-]InitClock: Error in getting current time\n");
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
        fprintf(Log_File, "[-]GetCurrTime: Invalid clock object\n");
        return -1;
    }
    struct timespec time;
    int err = clock_gettime(CLOCK_MONOTONIC_RAW, &time);
    if (CheckError(err, "[-]GetCurrTime: Error in getting current time"))
    {
        fprintf(Log_File, "[-]GetCurrTime: Error in getting current time\n");
        return -1;
    }
    return (time.tv_sec + time.tv_nsec * 1e-9) - (Clock->bootTime);
}

/**
 * @brief Recursive Helper function to populate the trie with the contents of the cwd.
 * @param root: The root node of the trie.
 * @return: 0 on success, -1 on failure.
 */
int Populate_Trie(Trie *root, char *dir)
{
    // Open the directory and add all files and folders to the trie
    struct dirent *entry, **namelist;
    int Num_Entries = scandir(dir, &namelist, NULL, alphasort);
    if (CheckError(Num_Entries, "[-]scandir: Error in getting directory entries"))
    {
        fprintf(Log_File, "[-]scandir: Error in getting directory entries");
        return -1;
    }

    for (int i = 0; i < Num_Entries; i++)
    {
        // Get the name of the file/folder
        char *name = namelist[i]->d_name;

        // Ignore the current and parent directory
        if (strncmp(name, ".", 1) == 0 || strncmp(name, "..", 2) == 0)
            continue;

        // Get the path of the file/folder
        char path[MAX_BUFFER_SIZE];
        char dir_path[MAX_BUFFER_SIZE];
        memset(path, 0, MAX_BUFFER_SIZE);
        snprintf(path, MAX_BUFFER_SIZE, "%s/%s", dir, name);
        strncpy(dir_path, path, MAX_BUFFER_SIZE);

        // Add the file/folder to the trie
        int err = trie_insert(root, path);
        if (CheckError(err, "[-]Populate_Trie: Error in adding file/folder to trie"))
        {
            fprintf(Log_File, "[-]Populate_Trie: Error in adding file/folder to trie\n");
            return -1;
        }

        // If the entry is a folder, then recursively add it's contents to the trie
        if (namelist[i]->d_type == DT_DIR)
        {
            err += Populate_Trie(root, dir_path);
            if (CheckError(err, "[-]Populate_Trie: Error in populating trie"))
            {
                printf("Error Path: %s\n", path);
                fprintf(Log_File, "[-]Populate_Trie: Error in recursively adding folder contents to trie (Path: %s) [Time Stamp: %f]\n", path, GetCurrTime(Clock));
            }
        }

        if (err < 0)
        {
            printf("[+]Populate_Trie: Error Detected while populating %d paths\n", -1 * err);
            fprintf(Log_File, "[+]Populate_Trie: Error Detected while populating %d paths [Time Stamp: %f]\n", -1 * err, GetCurrTime(Clock));
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Initializes the file trie with the contents of the cwd.
 * @return: A pointer to the root node of the trie on success, NULL on failure.
 * @note: The trie is populated with the all contents of the cwd(recursively)
 */
Trie *Initialize_File_Trie()
{
    // Initialize the trie
    Trie *root = trie_init();
    if (CheckNull(root, "[-]Initialize_File_Trie: Error in initializing trie"))
    {
        fprintf(Log_File, "[-]Initialize_File_Trie: Error in initializing trie\n");
        return NULL;
    }
    strcpy(root->path_token, "Mount");
    // Get the cwd
    char cwd[MAX_BUFFER_SIZE];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        fprintf(Log_File, "[-]Initialize_File_Trie: Error in getting cwd\n");
        return NULL;
    }
    printf("[+]Initialize_File_Trie: Trie initialized at path:\n %s (CWD)\n", cwd);

    // Populate the trie with the contents of the cwd (recursive)
    int err = Populate_Trie(root, ".");
    if (CheckError(err, "[-]Initialize_File_Trie: Error in populating trie"))
    {
        fprintf(Log_File, "[-]Initialize_File_Trie: Error in populating trie\n");
        return NULL;
    }

    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, MAX_BUFFER_SIZE);
    err = trie_print(root, buffer, 0);
    if (CheckError(err, "[-]Initialize_File_Trie: Error in getting mount paths"))
    {
        fprintf(Log_File, "[-]Initialize_File_Trie: Error in getting mount paths\n");
        return NULL;
    }
    printf("[+]Initialize_File_Trie: Mount Paths Hosted: \n%s\n", buffer);
    return root;
}

/**
 * @brief Thread to listen and handle requests from the Naming Server.
 * @param arg: The port number to listen on.
 * @return: NULL
 **/
void *NS_Listner_Thread(void *arg)
{
    int NSPort = *(int *)arg;

    // Socket for listening to Name Server
    int NS_Listen_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (CheckError(NS_Listen_Socket, "[-]NS_Listner_Thread: Error in creating socket for listening to Name Server"))
    {
        fprintf(Log_File, "[-]NS_Listner_Thread: Error in creating socket for listening to Name Server [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    // Address of Socket
    struct sockaddr_in NS_Listen_Addr;
    NS_Listen_Addr.sin_family = AF_INET;
    NS_Listen_Addr.sin_port = htons(NSPort);
    NS_Listen_Addr.sin_addr.s_addr = INADDR_ANY;
    memset(NS_Listen_Addr.sin_zero, '\0', sizeof(NS_Listen_Addr.sin_zero));

    // Bind the socket to the address
    int err = bind(NS_Listen_Socket, (struct sockaddr *)&NS_Listen_Addr, sizeof(NS_Listen_Addr));
    if (CheckError(err, "[-]NS_Listner_Thread: Error in binding socket to address"))
    {
        fprintf(Log_File, "[-]NS_Listner_Thread: Error in binding socket to address [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    err = listen(NS_Listen_Socket, 5);
    if (CheckError(err, "[-]NS_Listner_Thread: Error in listening for connections"))
    {
        fprintf(Log_File, "[-]NS_Listner_Thread: Error in listening for connections [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    printf("[+]NS_Listner_Thread: Listening for connections on Port: %d\n", NSPort);
    fprintf(Log_File, "[+]NS_Listner_Thread: Listening for connections on Port: %d [Time Stamp: %f]\n", NSPort, GetCurrTime(Clock));

    // Accept connections
    struct sockaddr_in NS_Client_Addr;
    socklen_t NS_Client_Addr_Size = sizeof(NS_Client_Addr);
    int NS_Client_Socket = accept(NS_Listen_Socket, (struct sockaddr *)&NS_Client_Addr, &NS_Client_Addr_Size);

    char *ns_IP = inet_ntoa(NS_Client_Addr.sin_addr);
    int ns_Port = ntohs(NS_Client_Addr.sin_port);

    if (CheckError(NS_Client_Socket, "[-]NS_Listner_Thread: Error in accepting connections"))
    {
        fprintf(Log_File, "[-]NS_Listner_Thread: Error in accepting connections [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    else if (strncmp(ns_IP, NS_IP, IP_LENGTH) != 0)
    {
        printf(RED "[-]NS_Listner_Thread: Connection Rejected from %s:%d\n" CRESET, ns_IP, ns_Port);
        fprintf(Log_File, "[-]NS_Listner_Thread: Connection Rejected from %s:%d [Time Stamp: %f]\n", ns_IP, ns_Port, GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    printf(GRN "[+]NS_Listner_Thread: Connection Established with Naming Server\n" CRESET);
    fprintf(Log_File, "[+]NS_Listner_Thread: Connection Established with Naming Server [Time Stamp: %f]\n", GetCurrTime(Clock));

    while (IsSocketConnected(NS_Client_Socket))
    {
        REQUEST_STRUCT NS_Response_Struct;
        REQUEST_STRUCT *NS_Response = &NS_Response_Struct;

        // Receive the request from the Name Server
        int err = recv(NS_Client_Socket, NS_Response, sizeof(RESPONSE_STRUCT), 0);
        if (CheckError(err, "[-]NS_Listner_Thread: Error in receiving data from Name Server"))
        {
            fprintf(Log_File, "[-]NS_Listner_Thread: Error in receiving data from Name Server [Time Stamp: %f]\n", GetCurrTime(Clock));
            exit(EXIT_FAILURE);
        }
        else if (err == 0)
        {
            printf(RED "[-]NS_Listner_Thread: Connection with Name Server Closed\n" CRESET);
            fprintf(Log_File, "[-]NS_Listner_Thread: Connection with Name Server Closed [Time Stamp: %f]\n", GetCurrTime(Clock));
            break;
        }

        // Print the request received from the Name Server
        printf(GRN "[+]NS_Listner_Thread: Request Received from Name Server\n" CRESET);
        fprintf(Log_File, "[+]NS_Listner_Thread: Request Received from Name Server [Time Stamp: %f]\n", GetCurrTime(Clock));
        printf("Request Operation: %d\n", NS_Response->iRequestOperation);
        printf("Request Path: %s\n", NS_Response->sRequestPath);
        printf("Request Flag: %d\n", NS_Response->iRequestFlags);
        printf("Request Client ID: %lu\n", NS_Response->iRequestClientID);

        RESPONSE_STRUCT NS_Request_Struct;
        RESPONSE_STRUCT *NS_Request = &NS_Request_Struct;
        memset(NS_Request, 0, sizeof(RESPONSE_STRUCT));
        NS_Request->iResponseOperation = NS_Response->iRequestOperation;
        NS_Request->iResponseFlags = NS_Response->iRequestFlags;
        NS_Request->iResponseServerID = Server_ID;

        switch (NS_Response->iRequestOperation)
        {
        case CMD_READ:
        {
            // relsove the path and send the contents of file in a single buffer

            break;
        }
        case CMD_WRITE:
        {
            break;
        }
        case CMD_INFO:
        {
            break;
        }
        case CMD_CREATE:
        {
            break;
        }
        case CMD_DELETE:
        {
            break;
        }
        case CMD_COPY:
        {
            break;
        }
        case CMD_RENAME:
        {
            // resolve the path and rename requested path to new path
            // send an ack to server after completion

            char *file_path = NS_Response->sRequestPath;
            char *new_name = __strtok_r(file_path, " ", &file_path);

            int present = trie_search(File_Trie, file_path);
            if (!present)
            {
                NS_Request->iResponseErrorCode = ERROR_INVALID_PATH;
                strncpy(NS_Request->sResponseData, "File Not Found", MAX_BUFFER_SIZE);
                printf(RED "[-]NS_Listner_Thread: File Not Found\n" CRESET);
                fprintf(Log_File, "[-]NS_Listner_Thread: File Not Found [Time Stamp: %f]\n", GetCurrTime(Clock));
                break;
            }

            char path_cpy[MAX_BUFFER_SIZE];
            strncpy(path_cpy, file_path, MAX_BUFFER_SIZE);

            // Get the corresponding Lock for the file
            Reader_Writer_Lock *lock = trie_get_path_lock(File_Trie, path_cpy);

            // Remove first token from the path (Mount)
            char *path = NULL;
            __strtok_r(path_cpy, "/", &path);

            // Update the trie with the new path
            int err = trie_rename(File_Trie, file_path, new_name);
            if (err < 0)
            {
                NS_Request->iResponseErrorCode = ERROR_INVALID_OPERATION;
                strncpy(NS_Request->sResponseData, "Error in renaming file", MAX_BUFFER_SIZE);
                printf(RED "[-]NS_Listner_Thread: Error in renaming file\n" CRESET);
                fprintf(Log_File, "[-]NS_Listner_Thread: Error in renaming file [Time Stamp: %f]\n", GetCurrTime(Clock));
                break;
            }

            Write_Lock(lock);
            err = rename(path, new_name);
            Write_Unlock(lock);

            if (err < 0)
            {
                NS_Request->iResponseErrorCode = ERROR_INVALID_OPERATION;
                strncpy(NS_Request->sResponseData, "Error in renaming file", MAX_BUFFER_SIZE);
                printf(RED "[-]NS_Listner_Thread: Error in renaming file\n" CRESET);
                fprintf(Log_File, "[-]NS_Listner_Thread: Error in renaming file [Time Stamp: %f]\n", GetCurrTime(Clock));
                break;
            }

            NS_Request->iResponseErrorCode = ERROR_CODE_SUCCESS;
            snprintf(NS_Request->sResponseData, MAX_BUFFER_SIZE, "File Renamed Successfully %lu", NS_Response->iRequestClientID);

            printf(GRN "[+]NS_Listner_Thread: File Renamed Successfully\n" CRESET);

            break;
        }
        case CMD_LIST:
        {
            break;
        }
        case CMD_MOVE:
        {
            break;
        }
        default:
        {
            NS_Request->iResponseErrorCode = ERROR_INVALID_OPERATION;
            strncpy(NS_Request->sResponseData, "Invalid Operation", MAX_BUFFER_SIZE);
            printf(RED "[-]NS_Listner_Thread: Invalid Operation\n" CRESET);
            fprintf(Log_File, "[-]NS_Listner_Thread: Invalid Operation [Time Stamp: %f]\n", GetCurrTime(Clock));
            break;
        }
        }

        // Send the response to the Name Server
        err = send(NS_Client_Socket, NS_Request, sizeof(RESPONSE_STRUCT), 0);
        if (CheckError(err, "[-]NS_Listner_Thread: Error in sending data to Name Server"))
        {
            fprintf(Log_File, "[-]NS_Listner_Thread: Error in sending data to Name Server [Time Stamp: %f]\n", GetCurrTime(Clock));
            exit(EXIT_FAILURE);
        }
        printf(GRN "[+]NS_Listner_Thread: Response Sent to Name Server\n" CRESET);
        fprintf(Log_File, "[+]NS_Listner_Thread: Response Sent to Name Server [Time Stamp: %f]\n", GetCurrTime(Clock));

    }
    return NULL;
}

/**
 * @brief Thread to listen and handle requests from the Client.
 * @param arg: The port number to listen on.
 * @return: NULL
 **/
void *Client_Listner_Thread(void *arg)
{
    int ClientPort = *(int *)arg;

    // Socket for listening to Client
    int Client_Listen_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (CheckError(Client_Listen_Socket, "[-]Client_Listner_Thread: Error in creating socket for listening to Client"))
    {
        fprintf(Log_File, "[-]Client_Listner_Thread: Error in creating socket for listening to Client [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    // Address of Socket
    struct sockaddr_in Client_Listen_Addr;
    Client_Listen_Addr.sin_family = AF_INET;
    Client_Listen_Addr.sin_port = htons(ClientPort);
    Client_Listen_Addr.sin_addr.s_addr = INADDR_ANY;
    memset(Client_Listen_Addr.sin_zero, '\0', sizeof(Client_Listen_Addr.sin_zero));

    // Bind the socket to the address
    int err = bind(Client_Listen_Socket, (struct sockaddr *)&Client_Listen_Addr, sizeof(Client_Listen_Addr));
    if (CheckError(err, "[-]Client_Listner_Thread: Error in binding socket to address"))
    {
        fprintf(Log_File, "[-]Client_Listner_Thread: Error in binding socket to address [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    err = listen(Client_Listen_Socket, MAX_CONN_Q);
    if (CheckError(err, "[-]Client_Listner_Thread: Error in listening for connections"))
    {
        fprintf(Log_File, "[-]Client_Listner_Thread: Error in listening for connections [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    printf("[+]Client_Listner_Thread: Listening for connections on Port: %d\n", ClientPort);
    fprintf(Log_File, "[+]Client_Listner_Thread: Listening for connections on Port: %d [Time Stamp: %f]\n", ClientPort, GetCurrTime(Clock));

    struct sockaddr_in Client_Addr;
    socklen_t Client_Addr_Size = sizeof(Client_Addr);
    int Client_Socket;
    // Accept connections and handle requests concurrently
    while (Client_Socket = accept(Client_Listen_Socket, (struct sockaddr *)&Client_Addr, &Client_Addr_Size))
    {
        char *client_IP = inet_ntoa(Client_Addr.sin_addr);
        int client_Port = ntohs(Client_Addr.sin_port);

        Client client;
        client.socket = Client_Socket;
        client.IP = client_IP;
        client.port = client_Port;

        if (CheckError(Client_Socket, "[-]Client_Listner_Thread: Error in accepting connections"))
        {
            fprintf(Log_File, "[-]Client_Listner_Thread: Error in accepting connections [Time Stamp: %f]\n", GetCurrTime(Clock));
            exit(EXIT_FAILURE);
        }

        printf(GRN "[+]Client_Listner_Thread: Connection Established with Client\n" CRESET);
        fprintf(Log_File, "[+]Client_Listner_Thread: Connection Established with Client [Time Stamp: %f]\n", GetCurrTime(Clock));

        // Create a thread to handle the request
        pthread_t Client_Handler;
        err = pthread_create(&Client_Handler, NULL, Client_Handler_Thread, (void *)&client);
        if (CheckError(err, "[-]Client_Listner_Thread: Error in creating thread for handling client request"))
        {
            fprintf(Log_File, "[-]Client_Listner_Thread: Error in creating thread for handling client request [Time Stamp: %f]\n", GetCurrTime(Clock));
            exit(EXIT_FAILURE);
        }
        fprintf(Log_File, "[+]Client_Listner_Thread: Thread Created for handling client request [Time Stamp: %f]\n", GetCurrTime(Clock));
    }

    return NULL;
}

/**
 * @brief Thread to handle requests from the Client.
 * @param arg: The socket to communicate with the client.
 * @return: NULL
 */
void *Client_Handler_Thread(void *arg)
{
    Client client = *(Client *)arg;
    int Client_Socket = client.socket;
    char *client_IP = client.IP;
    int client_Port = client.port;

    // Receive the request from the Client
    REQUEST_STRUCT Client_Request;
    REQUEST_STRUCT *Client_Request_Struct = &Client_Request;
    int err = recv(Client_Socket, Client_Request_Struct, sizeof(REQUEST_STRUCT), 0);
    if (err < 0)
    {
        printf(RED "[-]Client_Handler_Thread: Error in receiving data from Client (IP: %s, Port: %d)\n" CRESET, client_IP, client_Port);
        fprintf(Log_File, "[-]Client_Handler_Thread: Error in receiving data from Client (IP: %s, Port: %d) [Time Stamp: %f]\n", client_IP, client_Port, GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    else if (err == 0)
    {
        printf(RED "[-]Client_Handler_Thread: Connection with Client Closed Unexpectedly\n" CRESET);
        fprintf(Log_File, "[-]Client_Handler_Thread: Connection with Client Closed Unexpectedly [Time Stamp: %f]\n", GetCurrTime(Clock));
        return NULL;
    }

    // Print the request received from the Client
    printf(GRN "[+]Client_Handler_Thread: Request Received from Client (IP: %s, Port: %d)\n" CRESET, client_IP, client_Port);
    fprintf(Log_File, "[+]Client_Handler_Thread: Request Received from Client (IP: %s, Port: %d) [Time Stamp: %f]\n", client_IP, client_Port, GetCurrTime(Clock));
    printf("Request Operation: %d\n", Client_Request_Struct->iRequestOperation);
    printf("Request Path: %s\n", Client_Request_Struct->sRequestPath);
    printf("Request Flag: %d\n", Client_Request_Struct->iRequestFlags);
    printf("Request Client ID: %lu\n", Client_Request_Struct->iRequestClientID);

    RESPONSE_STRUCT Client_Response;
    RESPONSE_STRUCT *Client_Response_Struct = &Client_Response;
    memset(Client_Response_Struct, 0, sizeof(RESPONSE_STRUCT));
    Client_Response_Struct->iResponseOperation = Client_Request_Struct->iRequestOperation;
    Client_Response_Struct->iResponseFlags = Client_Request_Struct->iRequestFlags;
    Client_Response_Struct->iResponseServerID = Server_ID;

    switch (Client_Request_Struct->iRequestOperation)
    {
    case CMD_READ:
    {
        // generate a random stop sequence
        char stop_sequence[MAX_BUFFER_SIZE];
        memset(stop_sequence, 0, MAX_BUFFER_SIZE);
        snprintf(stop_sequence, MAX_BUFFER_SIZE, "STOP%d", rand() % 1000);

        // send the stop sequence to the client
        send(Client_Socket, stop_sequence, MAX_BUFFER_SIZE, 0);

        // Check if the file is exposed by the server
        char file_path[MAX_BUFFER_SIZE];
        memset(file_path, 0, MAX_BUFFER_SIZE);

        strncpy(file_path, Client_Request_Struct->sRequestPath, MAX_BUFFER_SIZE);

        int present = trie_search(File_Trie, Client_Request_Struct->sRequestPath);
        if (!present)
        {
            Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_PATH;
            strncpy(Client_Response_Struct->sResponseData, "File Not Found", MAX_BUFFER_SIZE);
            printf(RED "[-]Client_Handler_Thread: File Not Found\n" CRESET);
            fprintf(Log_File, "[-]Client_Handler_Thread: File Not Found [Time Stamp: %f]\n", GetCurrTime(Clock));

            // send a error buffer to indicate file not found
            char msg[] = RED "Error Fetching File" reset "\n";
            printf("%s\n", msg);
            send(Client_Socket, &msg, sizeof(msg), 0);
            send(Client_Socket, stop_sequence, MAX_BUFFER_SIZE, 0);

            break;
        }

        // Get the corresponding Lock for the file
        Reader_Writer_Lock *lock = trie_get_path_lock(File_Trie, file_path);

        memset(file_path, 0, MAX_BUFFER_SIZE);
        strncpy(file_path, Client_Request_Struct->sRequestPath, MAX_BUFFER_SIZE);

        // Remove first token from the path (Mount)
        char *path = NULL;
        __strtok_r(file_path, "/", &path);

        Read_Lock(lock);
        // Open the file and read it's contents
        FILE *file = fopen(path, "r");
        if (CheckNull(file, "[-]Client_Handler_Thread: Error in opening file"))
        {
            Read_Unlock(lock);
            Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_ACCESS;
            strncpy(Client_Response_Struct->sResponseData, "File Not Found", MAX_BUFFER_SIZE);
            printf(RED "[-]Client_Handler_Thread: File Not Found\n" CRESET);
            fprintf(Log_File, "[-]Client_Handler_Thread: File Not Found [Time Stamp: %f]\n", GetCurrTime(Clock));

            // send a error buffer to indicate file not found
            char msg[] = RED "Error Fetching File" reset "\n";
            send(Client_Socket, &msg, sizeof(msg), 0);
            send(Client_Socket, stop_sequence, MAX_BUFFER_SIZE, 0);

            break;
        }

        char buffer[MAX_BUFFER_SIZE];
        memset(buffer, 0, MAX_BUFFER_SIZE);

        while (fread(buffer, 1, MAX_BUFFER_SIZE, file) > 0)
        {
            send(Client_Socket, buffer, MAX_BUFFER_SIZE, 0);
            memset(buffer, 0, MAX_BUFFER_SIZE);
        }

        Read_Unlock(lock);
        // send the stop sequence to the client to indicate end of file
        send(Client_Socket, stop_sequence, MAX_BUFFER_SIZE, 0);

        int err = ferror(file);
        if (err)
        {
            Client_Response_Struct->iResponseFlags = RESPONSE_FLAG_FAILURE;
            Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_ACCESS;
            strncpy(Client_Response_Struct->sResponseData, "Error in reading file", MAX_BUFFER_SIZE);
            printf(RED "[-]Client_Handler_Thread: Error in reading file\n" CRESET);
            fprintf(Log_File, "[-]Client_Handler_Thread: Error in reading file [Time Stamp: %f]\n", GetCurrTime(Clock));
            break;
        }

        Client_Response_Struct->iResponseErrorCode = ERROR_CODE_SUCCESS;
        strncpy(Client_Response_Struct->sResponseData, "File Read Successfully", MAX_BUFFER_SIZE);

        printf(GRN "[+]Client_Handler_Thread: File Read Successfully\n" CRESET);
        fprintf(Log_File, "[+]Client_Handler_Thread: File Read Successfully [Time Stamp: %f]\n", GetCurrTime(Clock));
    }
    case CMD_WRITE:
    {
        // parse the write flag
        int write_flag = Client_Request_Struct->iRequestFlags;
        if (write_flag != REQUEST_FLAG_APPEND && write_flag != REQUEST_FLAG_OVERWRITE)
        {
            Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_FLAG;
            strncpy(Client_Response_Struct->sResponseData, "Invalid Write Flag", MAX_BUFFER_SIZE);
            printf(RED "[-]Client_Handler_Thread: Invalid Write Flag\n" CRESET);
            fprintf(Log_File, "[-]Client_Handler_Thread: Invalid Write Flag [Time Stamp: %f]\n", GetCurrTime(Clock));
            break;
        }

        // generate a random stop sequence
        char stop_sequence[MAX_BUFFER_SIZE];
        memset(stop_sequence, 0, MAX_BUFFER_SIZE);
        snprintf(stop_sequence, MAX_BUFFER_SIZE, "STOP%d", rand() % 1000);

        // send the stop sequence to the client
        send(Client_Socket, stop_sequence, MAX_BUFFER_SIZE, 0);

        // Check if the file is exposed by the server
        char file_path[MAX_BUFFER_SIZE];
        memset(file_path, 0, MAX_BUFFER_SIZE);

        strncpy(file_path, Client_Request_Struct->sRequestPath, MAX_BUFFER_SIZE);

        int present = trie_search(File_Trie, Client_Request_Struct->sRequestPath);
        if (!present)
        {
            Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_PATH;
            strncpy(Client_Response_Struct->sResponseData, "File Not Found", MAX_BUFFER_SIZE);
            printf(RED "[-]Client_Handler_Thread: File Not Found\n" CRESET);
            fprintf(Log_File, "[-]Client_Handler_Thread: File Not Found [Time Stamp: %f]\n", GetCurrTime(Clock));

            // send a error buffer to indicate file not found
            char msg[] = RED "Error Fetching File" reset "\n";
            printf("%s\n", msg);
            send(Client_Socket, &msg, sizeof(msg), 0);
            send(Client_Socket, stop_sequence, MAX_BUFFER_SIZE, 0);

            break;
        }

        // Get the corresponding Lock for the file
        Reader_Writer_Lock *lock = trie_get_path_lock(File_Trie, file_path);

        memset(file_path, 0, MAX_BUFFER_SIZE);
        strncpy(file_path, Client_Request_Struct->sRequestPath, MAX_BUFFER_SIZE);

        // Remove first token from the path (Mount)
        char *path = NULL;
        __strtok_r(file_path, "/", &path);

        // Open the file and write to it with the specified flag
        char *mode = (write_flag == REQUEST_FLAG_OVERWRITE) ? "w" : "a";

        Write_Lock(lock);
        FILE *file = fopen(path, mode);
        if (CheckNull(file, "[-]Client_Handler_Thread: Error in opening file"))
        {
            Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_ACCESS;
            strncpy(Client_Response_Struct->sResponseData, "File Not Found", MAX_BUFFER_SIZE);
            printf(RED "[-]Client_Handler_Thread: File Not Found\n" CRESET);
            fprintf(Log_File, "[-]Client_Handler_Thread: File Not Found [Time Stamp: %f]\n", GetCurrTime(Clock));

            // send a error buffer to indicate file not found
            char msg[] = RED "Error Opening File" reset "\n";
            send(Client_Socket, &msg, sizeof(msg), 0);
            send(Client_Socket, stop_sequence, MAX_BUFFER_SIZE, 0);

            break;
        }

        char buffer[MAX_BUFFER_SIZE];
        memset(buffer, 0, MAX_BUFFER_SIZE);

        // receive the file contents from the client
        while (recv(Client_Socket, buffer, MAX_BUFFER_SIZE, 0) > 0)
        {
            // check if the stop sequence is received
            if (strncmp(buffer, stop_sequence, MAX_BUFFER_SIZE) == 0)
                break;

            size_t writeSize = fwrite(buffer, 1, strlen(buffer), file);
            printf("Writing %ld bytes to file\n", writeSize);
            fprintf(Log_File, "Writing %ld bytes to file\n", writeSize);
            memset(buffer, 0, MAX_BUFFER_SIZE);
        }

        Write_Unlock(lock);
        int err = ferror(file);
        if (err)
        {
            Client_Response_Struct->iResponseFlags = RESPONSE_FLAG_FAILURE;
            Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_ACCESS;
            strncpy(Client_Response_Struct->sResponseData, "Error in writing file", MAX_BUFFER_SIZE);
            printf(RED "[-]Client_Handler_Thread: Error in writing file\n" CRESET);
            fprintf(Log_File, "[-]Client_Handler_Thread: Error in writing file [Time Stamp: %f]\n", GetCurrTime(Clock));
            break;
        }

        fclose(file);

        Client_Response_Struct->iResponseErrorCode = ERROR_CODE_SUCCESS;
        strncpy(Client_Response_Struct->sResponseData, "File Written Successfully", MAX_BUFFER_SIZE);

        printf(GRN "[+]Client_Handler_Thread: File Written Successfully\n" CRESET);
        fprintf(Log_File, "[+]Client_Handler_Thread: File Written Successfully [Time Stamp: %f]\n", GetCurrTime(Clock));
        break;
    }
    case CMD_INFO:
    {
        // Check if the file is exposed by the server
        char file_path[MAX_BUFFER_SIZE];
        memset(file_path, 0, MAX_BUFFER_SIZE);

        strncpy(file_path, Client_Request_Struct->sRequestPath, MAX_BUFFER_SIZE);

        int present = trie_search(File_Trie, Client_Request_Struct->sRequestPath);
        if (!present)
        {
            Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_PATH;
            strncpy(Client_Response_Struct->sResponseData, "File Not Found", MAX_BUFFER_SIZE);
            printf(RED "[-]Client_Handler_Thread: File Not Found\n" CRESET);
            fprintf(Log_File, "[-]Client_Handler_Thread: File Not Found [Time Stamp: %f]\n", GetCurrTime(Clock));
            break;
        }

        // Get the corresponding Lock for the file
        Reader_Writer_Lock *lock = trie_get_path_lock(File_Trie, file_path);

        memset(file_path, 0, MAX_BUFFER_SIZE);
        strncpy(file_path, Client_Request_Struct->sRequestPath, MAX_BUFFER_SIZE);

        // Remove first token from the path (Mount)
        char *path = NULL;
        __strtok_r(file_path, "/", &path);

        PATH_INFO_STRUCT info;
        PATH_INFO_STRUCT *info_struct = &info;
        memset(info_struct, 0, sizeof(PATH_INFO_STRUCT));

        Read_Lock(lock);
        // Check if path is a file, executable or a directory
        struct stat file_stat;
        int err = stat(path, &file_stat);
        Read_Unlock(lock);

        if (err < 0)
        {
            Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_PATH;
            strncpy(Client_Response_Struct->sResponseData, "File Not Found", MAX_BUFFER_SIZE);
            printf(RED "[-]Client_Handler_Thread: File Not Found\n" CRESET);
            fprintf(Log_File, "[-]Client_Handler_Thread: File Not Found [Time Stamp: %f]\n", GetCurrTime(Clock));
            break;
        }

        // send the response to the client
        Client_Response_Struct->iResponseErrorCode = ERROR_CODE_SUCCESS;
        strncpy(Client_Response_Struct->sResponseData, "File Info Fetched Successfully", MAX_BUFFER_SIZE);

        send(Client_Socket, Client_Response_Struct, sizeof(RESPONSE_STRUCT), 0);

        // Populate Info Struct
        strncpy(info_struct->sPath, path, MAX_BUFFER_SIZE);
        info_struct->iPathType = file_stat.st_mode & __S_IFMT;
        info_struct->iPathPermission = file_stat.st_mode & 0777; // 0777 is the mask for permissions in octal
        info_struct->iPathSize = file_stat.st_size;
        info_struct->iPathModificationTime = file_stat.st_mtime;
        info_struct->iPathCreationTime = file_stat.st_ctime;
        info_struct->iPathAccessTime = file_stat.st_atime;
        info_struct->iPathLinks = file_stat.st_nlink;

        // send the info struct to the client
        send(Client_Socket, info_struct, sizeof(PATH_INFO_STRUCT), 0);

        printf(GRN "[+]Client_Handler_Thread: File Info Fetched Successfully\n" CRESET);
        fprintf(Log_File, "[+]Client_Handler_Thread: File Info Fetched Successfully [Time Stamp: %f]\n", GetCurrTime(Clock));

        return NULL;
    }
    case CMD_CREATE:
    case CMD_DELETE:
    case CMD_COPY:
    case CMD_RENAME:
    case CMD_LIST:
    case CMD_MOVE:
    {
        Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_AUTHENTICATION;
        strncpy(Client_Response_Struct->sResponseData, "Invalid Authentication", MAX_BUFFER_SIZE);
        printf(RED "[-]Client_Handler_Thread: Client Requested a Indirect Secure Operation\n" CRESET);
        fprintf(Log_File, "[-]Client_Handler_Thread: Client Requested a Indirect Secure Operation [Time Stamp: %f]\n", GetCurrTime(Clock));
        break;
    }
    default:
    {
        Client_Response_Struct->iResponseErrorCode = ERROR_INVALID_OPERATION;
        strncpy(Client_Response_Struct->sResponseData, "Invalid Operation", MAX_BUFFER_SIZE);
        printf(RED "[-]Client_Handler_Thread: Invalid Request Operation\n" CRESET);
        fprintf(Log_File, "[-]Client_Handler_Thread: Invalid Request Operation [Time Stamp: %f]\n", GetCurrTime(Clock));

        break;
    }
    }

    // Send the response to the Client
    err = send(Client_Socket, Client_Response_Struct, sizeof(RESPONSE_STRUCT), 0);
    if (err < 0)
    {
        printf(RED "[-]Client_Handler_Thread: Error in sending data to Client (IP: %s, Port: %d)\n" CRESET, client_IP, client_Port);
        fprintf(Log_File, "[-]Client_Handler_Thread: Error in sending data to Client (IP: %s, Port: %d) [Time Stamp: %f]\n", client_IP, client_Port, GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    else if (err == 0)
    {
        printf(RED "[-]Client_Handler_Thread: Connection with Client Closed Unexpectedly\n" CRESET);
        fprintf(Log_File, "[-]Client_Handler_Thread: Connection with Client Closed Unexpectedly [Time Stamp: %f]\n", GetCurrTime(Clock));
        return NULL;
    }

    printf(GRN "[+]Client_Handler_Thread: Response Sent to Client (IP: %s, Port: %d)\n" CRESET, client_IP, client_Port);
    fprintf(Log_File, "[+]Client_Handler_Thread: Response Sent to Client (IP: %s, Port: %d) [Time Stamp: %f]\n", client_IP, client_Port, GetCurrTime(Clock));

    return NULL;
}

/**
 * @brief Thread to flush the logs to the log file.
 * @note: The logs are flushed every LOG_FLUSH_INTERVAL seconds.
 */
void *Log_Flusher_Thread()
{
    while (1)
    {
        sleep(LOG_FLUSH_INTERVAL);
        printf(BBLK "[+]Log Flusher Thread: Flushing logs\n" reset);

        fprintf(Log_File, "[+]Log Flusher Thread: Flushing logs [Time Stamp: %f]\n", GetCurrTime(Clock));
        fprintf(Log_File, "------------------------------------------------------------\n");
        char buffer[MAX_BUFFER_SIZE];
        memset(buffer, 0, MAX_BUFFER_SIZE);
        int err = trie_print(File_Trie, buffer, 0);
        if (CheckError(err, "[-]Log_Flusher_Thread: Error in getting mount paths"))
        {
            fprintf(Log_File, "[-]Log_Flusher_Thread: Error in getting mount paths [Time Stamp: %f]\n", GetCurrTime(Clock));
            exit(EXIT_FAILURE);
        }
        fprintf(Log_File, "%s\n", buffer);
        fprintf(Log_File, "------------------------------------------------------------\n");

        fflush(Log_File);
    }
    return NULL;
}

/**
 * @brief Exit handler for the server.
 * @note: destroys the trie and closes the log file.
 */
void exit_handler()
{
    printf(BRED "[-]Server Exiting\n" reset);
    fprintf(Log_File, "[-]Server Exiting [Time Stamp: %f]\n", GetCurrTime(Clock));
    trie_destroy(File_Trie);
    fclose(Log_File);
    return;
}

int main()
{
    printf("Enter (2)Port Number You want to use for Communication:\t");
    int NSPort, ClientPort;
    scanf("%d %d", &NSPort, &ClientPort);

    // Register the exit handler
    atexit(exit_handler);

    // Initialize the log file
    Log_File = fopen("./SSlog.log", "w");
    if (Log_File == NULL)
    {
        fprintf(stderr, "Error opening log file\n");
        exit(1);
    }

    // Initialize the clock
    Clock = InitClock();
    if (CheckNull(Clock, "[-]main: Error in initializing clock"))
    {
        fprintf(Log_File, "[-]main: Error in initializing clock [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    fprintf(Log_File, "[+]Server Initialized [Time Stamp: %f]\n", GetCurrTime(Clock));
    printf("[+]Server Initialized\n");

    // Create a thread to flush the logs periodically
    pthread_t tLogFlusherThread;
    int iThreadStatus = pthread_create(&tLogFlusherThread, NULL, Log_Flusher_Thread, NULL);
    if (CheckError(iThreadStatus, "[-]Error in creating thread"))
        return 1;

    // Initialize trie for storing and saving all files exposed by the server (includes entire cwd structure)
    // If a folder is exposed, then it's children are also exposed
    // If a file is exposed, then it's path is stored in the trie
    File_Trie = Initialize_File_Trie();
    if (CheckNull(File_Trie, "[-]main: Error in initializing file trie"))
    {
        fprintf(Log_File, "[-]main: Error in initializing file trie [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    // Address to Name Server
    struct sockaddr_in NS_Addr;
    NS_Addr.sin_family = AF_INET;
    NS_Addr.sin_port = htons(NS_SERVER_PORT);
    NS_Addr.sin_addr.s_addr = inet_addr(NS_IP);
    memset(NS_Addr.sin_zero, '\0', sizeof(NS_Addr.sin_zero));

    // Socket for sending data to Name Server
    NS_Write_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (CheckError(NS_Write_Socket, "[-]main: Error in creating socket for sending data to Name Server"))
    {
        fprintf(Log_File, "[-]main: Error in creating socket for sending data to Name Server [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    int err = connect(NS_Write_Socket, (struct sockaddr *)&NS_Addr, sizeof(NS_Addr));
    if (CheckError(err, "[-]main: Error in connecting to Name Server"))
    {
        fprintf(Log_File, "[-]main: Error in connecting to Name Server [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("[+]Connection Established with Naming Server\n");
        fprintf(Log_File, "[+]Connection Established with Naming Server [Time Stamp: %f]\n", GetCurrTime(Clock));
    }
    STORAGE_SERVER_INIT_STRUCT Packet;
    STORAGE_SERVER_INIT_STRUCT *SS_Init_Struct = &Packet;

    /*
    STORAGE_SERVER_INIT_STRUCT *SS_Init_Struct = (STORAGE_SERVER_INIT_STRUCT *)malloc(sizeof(STORAGE_SERVER_INIT_STRUCT));
    if (CheckNull(SS_Init_Struct, "[-]main: Error in allocating memory"))
    {
        fprintf(Log_File, "[-]main: Error in allocating memory [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    */

    SS_Init_Struct->sServerPort_Client = ClientPort;
    SS_Init_Struct->sServerPort_NServer = NSPort;
    char root_path[MAX_BUFFER_SIZE] = "./";
    memset(SS_Init_Struct->MountPaths, 0, MAX_BUFFER_SIZE);
    err = trie_paths(File_Trie, SS_Init_Struct->MountPaths, root_path);
    if (CheckError(err, "[-]main: Error in getting mount paths"))
    {
        fprintf(Log_File, "[-]main: Error in getting mount paths [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    // printf("[+]main: Sent Mounted Paths: \n%s\n", SS_Init_Struct->MountPaths);

    err = send(NS_Write_Socket, SS_Init_Struct, sizeof(STORAGE_SERVER_INIT_STRUCT), 0);
    if (err != sizeof(STORAGE_SERVER_INIT_STRUCT))
    {
        fprintf(Log_File, "[-]main: Error in sending data to Name Server [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    fprintf(Log_File, "[+]Initialization Packet Sent to Name Server [Time Stamp: %f]\n", GetCurrTime(Clock));

    // receive the Server ID from the Name Server
    err = recv(NS_Write_Socket, &Server_ID, sizeof(unsigned long), 0);
    if (CheckError(err, "[-]main: Error in receiving data from Name Server"))
    {
        fprintf(Log_File, "[-]main: Error in receiving data from Name Server [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    else if (err == 0)
    {
        printf(RED "[-]main: Connection with Name Server Closed Unexpectedly\n" CRESET);
        fprintf(Log_File, "[-]main: Connection with Name Server Closed Unexpectedly [Time Stamp: %f]\n", GetCurrTime(Clock));
        return 1;
    }

    printf("[+]Connection Established with Naming Server\n");
    fprintf(Log_File, "[+]Connection Established with Naming Server [Time Stamp: %f]\n", GetCurrTime(Clock));

    printf(BWHT "[+]Server ID: %lu\n" CRESET, Server_ID);
    fprintf(Log_File, "[+]Server ID: %lu [Time Stamp: %f]\n", Server_ID, GetCurrTime(Clock));

    // Setup Listner for Name Server
    pthread_t NS_Listner;
    err = pthread_create(&NS_Listner, NULL, NS_Listner_Thread, (void *)&NSPort);
    if (CheckError(err, "[-]main: Error in creating thread for Name Server Listner"))
    {
        fprintf(Log_File, "[-]main: Error in creating thread for Name Server Listner [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    fprintf(Log_File, "[+]Name Server Listner Thread Created [Time Stamp: %f]\n", GetCurrTime(Clock));

    // Setup Listner for Client
    pthread_t Client_Listner;
    err = pthread_create(&Client_Listner, NULL, Client_Listner_Thread, (void *)&ClientPort);
    if (CheckError(err, "[-]main: Error in creating thread for Client Listner"))
    {
        fprintf(Log_File, "[-]main: Error in creating thread for Client Listner [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    fprintf(Log_File, "[+]Client Listner Thread Created [Time Stamp: %f]\n", GetCurrTime(Clock));

    // Wait for the threads to finish
    pthread_join(NS_Listner, NULL);
    pthread_join(Client_Listner, NULL);

    return 0;
}
