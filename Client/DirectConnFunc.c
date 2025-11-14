#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h> //inet_addr

// Custom Header Files
#include "../Externals.h"
#include "../colour.h"
#include "./Headers.h"
#include "./Hash.h"
#include "./ErrorCodes.h"

void Rcmd(char* arg, int ServerSockfd)
{
    if(CheckNull(arg, ErrorMsg("NULL Argument\nUSAGE: READ <Path>", CMD_ERROR_INVALID_ARGUMENTS)))
    {
        fprintf(Clientlog, "[-]Rcmd: Invalid Argument [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    arg = strtok(arg, " \t\n");
    if(strtok(NULL, " \t\n") != NULL)
    {
        fprintf(Clientlog, "[-]Rcmd: Invalid Argument Count [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    char* path = arg;
    fprintf(Clientlog, "[+]Rcmd: Reading Path %s [Time Stamp: %f]\n", path, GetCurrTime(Clock));

    REQUEST_STRUCT req_struct;
    REQUEST_STRUCT* req = &req_struct;
    memset(req, 0, sizeof(REQUEST_STRUCT));

    req->iRequestOperation = CMD_READ;
    req->iRequestClientID = iClientID;
    strncpy(req->sRequestPath, path, MAX_BUFFER_SIZE);
    // req->iRequestFlags = 0;

    int iBytesSent = send(ServerSockfd, req, sizeof(REQUEST_STRUCT), 0);

    if(iBytesSent != sizeof(REQUEST_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to send request to server", CMD_ERROR_SEND_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Rcmd: Failed to send request [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }
    
    RESPONSE_STRUCT res_struct;
    RESPONSE_STRUCT* res = &res_struct;
    memset(res, 0, sizeof(RESPONSE_STRUCT));

    int iBytesRecv = recv(ServerSockfd, res, sizeof(RESPONSE_STRUCT), 0);
    if(iBytesRecv != sizeof(RESPONSE_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to receive response from server", CMD_ERROR_RECV_FAILED);
        printf(RED"%s"reset, Msg);
        fprintf(Clientlog, "[-]Rcmd: Failed to receive response [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }
    
    if(res->iResponseFlags == RESPONSE_FLAG_FAILURE )
    {
        char* Msg = ErrorMsg("Failed to read file", res->iResponseErrorCode);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Rcmd: Failed to read file [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }
    else if(res->iResponseFlags == BACKUP_RESPONSE)
    {
        printf(YEL"Corresponding Storage Server is down. Trying to read from backup server\n"reset);
        fprintf(Clientlog, "[+]Rcmd: Corresponding Storage Server is down. Trying to read from backup server [Time Stamp: %f]\n", GetCurrTime(Clock));

        // Modify the path to the backup path
        memset(req->sRequestPath, 0, sizeof(req->sRequestPath));
        snprintf(req->sRequestPath, MAX_BUFFER_SIZE, "./backup%s", path);
    }
    // The response data is the IP and Port of the storage server serving the file seperated by a space
    char* ip = strtok(res->sResponseData, " ");
    char* port = strtok(NULL, " ");

    // Check if  IP and Port are valid
    if(CheckNull(ip, ErrorMsg("Invalid IP received from server", CMD_ERROR_INVALID_RECV_VALUE)))
    {
        fprintf(Clientlog, "[-]Rcmd: Invalid IP received from server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }
    else if(CheckNull(port, ErrorMsg("Invalid Port received from server", CMD_ERROR_INVALID_RECV_VALUE)))
    {
        fprintf(Clientlog, "[-]Rcmd: Invalid Port received from server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Connect to the storage server
    int StorageSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(CheckError(StorageSockfd, ErrorMsg("Failed to create socket", CMD_ERROR_SOCKET_FAILED)))
    {
        fprintf(Clientlog, "[-]Rcmd: Failed to create socket [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    struct sockaddr_in StorageServer;
    memset(&StorageServer, 0, sizeof(StorageServer));
    StorageServer.sin_family = AF_INET;
    StorageServer.sin_addr.s_addr = inet_addr(ip);
    StorageServer.sin_port = htons(atoi(port));
    memset(StorageServer.sin_zero, '\0', sizeof(StorageServer.sin_zero));

    int iConnectStatus = connect(StorageSockfd, (struct sockaddr *)&StorageServer, sizeof(StorageServer));
    if(CheckError(iConnectStatus, ErrorMsg("Failed to connect to storage server", CMD_ERROR_CONNECT_FAILED)))
    {
        fprintf(Clientlog, "[-]Rcmd: Failed to connect to storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Send the request to the storage server
    iBytesSent = send(StorageSockfd, req, sizeof(REQUEST_STRUCT), 0);
    if(iBytesSent != sizeof(REQUEST_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to send request to storage server", CMD_ERROR_SEND_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Rcmd: Failed to send request to storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Receive stop sequence from server
    char stop[MAX_BUFFER_SIZE];
    iBytesRecv = recv(StorageSockfd, stop, MAX_BUFFER_SIZE, 0);
    if(CheckError(iBytesRecv, ErrorMsg("Failed to receive stop sequence from storage server", CMD_ERROR_RECV_FAILED)))
    {
        fprintf(Clientlog, "[-]Rcmd: Failed to receive stop sequence from storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }
    // printf("Stop Sequence: %s\n", stop);

    // Receive the File from the storage server in chunks of MAX_BUFFER_SIZE until the server sends a chunk of size less than MAX_BUFFER_SIZE
    long long int FileSize = 0;
    printf("File Contents:\n"MAG"----------------------------------------\n");
    while(1)
    {
        char buffer[MAX_BUFFER_SIZE];
        memset(buffer, 0, MAX_BUFFER_SIZE);

        iBytesRecv = recv(StorageSockfd, buffer, MAX_BUFFER_SIZE, 0);
        if(CheckError(iBytesRecv, ErrorMsg("Failed to receive file from storage server", CMD_ERROR_RECV_FAILED)))
        {
            fprintf(Clientlog, "[-]Rcmd: Failed to receive file from storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
            return;
        }

        // Check if the server has sent the stop sequence
        if(strncmp(buffer, stop, MAX_BUFFER_SIZE) == 0)
        {
            break;
        }

        // print the recieved data
        printf("%s", buffer);
        FileSize += strlen(buffer);

    }
    
    printf("\n----------------------------------------\n"reset);
    printf("Read Bytes: %lld Bytes\n", FileSize);
    // Receive the response from the storage server
    iBytesRecv = recv(StorageSockfd, res, sizeof(RESPONSE_STRUCT), 0);
    if(iBytesRecv != sizeof(RESPONSE_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to receive response from storage server", CMD_ERROR_RECV_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Rcmd: Failed to receive response from storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    if(res->iResponseFlags == RESPONSE_FLAG_FAILURE)
    {
        char* Msg = ErrorMsg("Failed to read file from storage server", res->iResponseErrorCode);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Rcmd: Failed to read file from storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // log the response
    fprintf(Clientlog, "[+]Rcmd: Server Response: %s [Time Stamp: %f]\n", res->sResponseData, GetCurrTime(Clock));

    // Close the socket
    close(StorageSockfd);
    fprintf(Clientlog, "[+]Rcmd: Successfully read file [Time Stamp: %f]\n", GetCurrTime(Clock));
    return;
}
void Wcmd(char* arg, int ServerSockfd)
{
    if(CheckNull(arg, ErrorMsg("NULL Argument\nUSAGE: WRITE <Flag> <Path>", CMD_ERROR_INVALID_ARGUMENTS)))
    {
        fprintf(Clientlog, "[-]Wcmd: Invalid Argument [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Divide the argument into flag and path
    char* flag = strtok(arg, "- \t\n");
    char* path = strtok(NULL, " \t\n");

    // process the flag 
    // 0 for append(default), 1 for overwrite
    int iFlag = REQUEST_FLAG_APPEND; 
    if(flag != NULL)
    {
        if(strncmp(flag, "a", 1) == 0)
        {
            iFlag = REQUEST_FLAG_APPEND;
        }
        else if(strncmp(flag, "o", 1) == 0)
        {
            iFlag = REQUEST_FLAG_OVERWRITE;
        }
        else
        {
            char* Msg = ErrorMsg("Invalid Flag\nUSAGE: WRITE <Flag> <Path>\nFlag: a for append, o for overwrite", CMD_ERROR_INVALID_ARGUMENTS);
            printf(RED"%s\n"reset, Msg);
            fprintf(Clientlog, "[-]Wcmd: Invalid Flag [Time Stamp: %f]\n", GetCurrTime(Clock));
            free(Msg);
            return;
        }
        flag = strtok(NULL, "-");
    }

    // Check if the path is valid
    if(CheckNull(path, ErrorMsg("Invalid Path\nUSAGE: WRITE <Flag> <Path>", CMD_ERROR_INVALID_ARGUMENTS)))
    {
        fprintf(Clientlog, "[-]Wcmd: Invalid Path [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Check if there are any extra arguments
    if(strtok(NULL, " \t\n") != NULL)
    {
        char* Msg = ErrorMsg("Invalid Argument Count\nUSAGE: WRITE <Flag> <Path>", CMD_ERROR_INVALID_ARGUMENTS_COUNT);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Wcmd: Invalid Argument Count [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Construst the request
    REQUEST_STRUCT req_struct;
    REQUEST_STRUCT* req = &req_struct;
    memset(req, 0, sizeof(REQUEST_STRUCT));

    req->iRequestOperation = CMD_WRITE;
    req->iRequestClientID = iClientID;
    req->iRequestFlags = iFlag;
    strncpy(req->sRequestPath, path, MAX_BUFFER_SIZE);
    
    // Send the request to the server
    int iBytesSent = send(ServerSockfd, req, sizeof(REQUEST_STRUCT), 0);
    if(iBytesSent != sizeof(REQUEST_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to send request to server", CMD_ERROR_SEND_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Wcmd: Failed to send request to server [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Receive the response from the server
    RESPONSE_STRUCT res_struct;
    RESPONSE_STRUCT* res = &res_struct;
    memset(res, 0, sizeof(RESPONSE_STRUCT));

    int iBytesRecv = recv(ServerSockfd, res, sizeof(RESPONSE_STRUCT), 0);
    if(iBytesRecv != sizeof(RESPONSE_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to receive response from server", CMD_ERROR_RECV_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Wcmd: Failed to receive response from server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    if(res->iResponseFlags == RESPONSE_FLAG_FAILURE)
    {
        char* Msg = ErrorMsg("Failed to write file", res->iResponseErrorCode);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Wcmd: Failed to write file [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }
    else if(res->iResponseFlags == BACKUP_RESPONSE)
    {
        printf(YEL"Corresponding Storage Server is down.\n"reset);
        fprintf(Clientlog, "[+]Wcmd: Corresponding Storage Server is down.[Time Stamp: %f]\n", GetCurrTime(Clock));

        char* Msg = ErrorMsg("Failed to write file", res->iResponseErrorCode);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Wcmd: Failed to write file [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // The response data is the IP and Port of the storage server serving the file seperated by a space
    char* ip = strtok(res->sResponseData, " ");
    char* port = strtok(NULL, " ");

    // Check if  IP and Port are valid
    if(CheckNull(ip, ErrorMsg("Invalid IP received from server", CMD_ERROR_INVALID_RECV_VALUE)))
    {
        fprintf(Clientlog, "[-]Rcmd: Invalid IP received from server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }
    else if(CheckNull(port, ErrorMsg("Invalid Port received from server", CMD_ERROR_INVALID_RECV_VALUE)))
    {
        fprintf(Clientlog, "[-]Rcmd: Invalid Port received from server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Connect to the storage server
    int StorageSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(CheckError(StorageSockfd, ErrorMsg("Failed to create socket", CMD_ERROR_SOCKET_FAILED)))
    {
        fprintf(Clientlog, "[-]Rcmd: Failed to create socket [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    struct sockaddr_in StorageServer;
    memset(&StorageServer, 0, sizeof(StorageServer));
    StorageServer.sin_family = AF_INET;
    StorageServer.sin_addr.s_addr = inet_addr(ip);
    StorageServer.sin_port = htons(atoi(port));
    memset(StorageServer.sin_zero, '\0', sizeof(StorageServer.sin_zero));

    int iConnectStatus = connect(StorageSockfd, (struct sockaddr *)&StorageServer, sizeof(StorageServer));
    if(CheckError(iConnectStatus, ErrorMsg("Failed to connect to storage server", CMD_ERROR_CONNECT_FAILED)))
    {
        fprintf(Clientlog, "[-]Rcmd: Failed to connect to storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Send the request to the storage server
    iBytesSent = send(StorageSockfd, req, sizeof(REQUEST_STRUCT), 0);
    if(iBytesSent != sizeof(REQUEST_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to send request to storage server", CMD_ERROR_SEND_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Wcmd: Failed to send request to storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Receive stop sequence from server
    char stop[MAX_BUFFER_SIZE];
    iBytesRecv = recv(StorageSockfd, stop, MAX_BUFFER_SIZE, 0);
    if(CheckError(iBytesRecv, ErrorMsg("Failed to receive stop sequence from storage server", CMD_ERROR_RECV_FAILED)))
    {
        fprintf(Clientlog, "[-]Wcmd: Failed to receive stop sequence from storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }
    // printf("Stop Sequence: %s\n", stop);

    // Take the input from the user and send it to the storage server
    printf("\n"GRN"Enter the data to be written to the file. Press Ctrl+D to stop\n"reset);
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, MAX_BUFFER_SIZE);
    while(fgets(buffer, MAX_BUFFER_SIZE, stdin) != NULL)
    {
        int iBytesSent = send(StorageSockfd, buffer, MAX_BUFFER_SIZE, 0);
        if(CheckError(iBytesSent, ErrorMsg("Failed to send data to storage server", CMD_ERROR_SEND_FAILED)))
        {
            fprintf(Clientlog, "[-]Wcmd: Failed to send data to storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
            return;
        }
        memset(buffer, 0, MAX_BUFFER_SIZE);
        if(feof(stdin)) break;
        else if(ferror(stdin)) {
            printf(RED"Error reading from stdin\n"reset);
            fprintf(Clientlog, "[-]Wcmd: Error reading from stdin [Time Stamp: %f]\n", GetCurrTime(Clock));
            return;
        }
    }
    
    // clear the EOF flag
    clearerr(stdin);

    // Send the stop sequence to the server
    iBytesSent = send(StorageSockfd, stop, MAX_BUFFER_SIZE, 0);
    if(CheckError(iBytesSent, ErrorMsg("Failed to send stop sequence to storage server", CMD_ERROR_SEND_FAILED)))
    {
        fprintf(Clientlog, "[-]Wcmd: Failed to send stop sequence to storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Receive the response from the storage server
    iBytesRecv = recv(StorageSockfd, res, sizeof(RESPONSE_STRUCT), 0);
    if(iBytesRecv != sizeof(RESPONSE_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to receive response from storage server", CMD_ERROR_RECV_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Wcmd: Failed to receive response from storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    if(res->iResponseFlags == RESPONSE_FLAG_FAILURE)
    {
        char* Msg = ErrorMsg("Failed to write file to storage server", res->iResponseErrorCode);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Wcmd: Failed to write file to storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // log the response
    fprintf(Clientlog, "[+]Wcmd: Server Response: %s [Time Stamp: %f]\n", res->sResponseData, GetCurrTime(Clock));

    close(StorageSockfd);
    fprintf(Clientlog, "[+]Wcmd: Successfully wrote file [Time Stamp: %f]\n", GetCurrTime(Clock));
    printf(GRN"File wrote to successfully\n"reset);

    return;
}
void Icmd(char* arg, int ServerSockfd)
{
    if(CheckNull(arg, ErrorMsg("NULL Argument\nUSAGE: INFO <Path>", CMD_ERROR_INVALID_ARGUMENTS)))
    {
        fprintf(Clientlog, "[-]Icmd: Invalid Argument [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    arg = strtok(arg, " \t\n");
    if(strtok(NULL, " \t\n") != NULL)
    {
        printf(RED"Invalid Argument Count\nUSAGE: INFO <Path>\n"reset);
        fprintf(Clientlog, "[-]Icmd: Invalid Argument Count [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    char* path = arg;
    fprintf(Clientlog, "[+]Icmd: Describing Path %s [Time Stamp: %f]\n", path, GetCurrTime(Clock));

    REQUEST_STRUCT req_struct;
    REQUEST_STRUCT* req = &req_struct;
    memset(req, 0, sizeof(REQUEST_STRUCT));

    req->iRequestOperation = CMD_INFO;
    req->iRequestClientID = iClientID;
    strncpy(req->sRequestPath, path, MAX_BUFFER_SIZE);

    int iBytesSent = send(ServerSockfd, req, sizeof(REQUEST_STRUCT), 0);
    if(iBytesSent != sizeof(REQUEST_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to send request to server", CMD_ERROR_SEND_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Icmd: Failed to send request [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    RESPONSE_STRUCT res_struct;
    RESPONSE_STRUCT* res = &res_struct;
    memset(res, 0, sizeof(RESPONSE_STRUCT));

    int iBytesRecv = recv(ServerSockfd, res, sizeof(RESPONSE_STRUCT), 0);
    if(iBytesRecv != sizeof(RESPONSE_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to receive response from server", CMD_ERROR_RECV_FAILED);
        printf(RED"%s"reset, Msg);
        fprintf(Clientlog, "[-]Icmd: Failed to receive response [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    if(res->iResponseFlags == RESPONSE_FLAG_FAILURE)
    {
        char* Msg = ErrorMsg("Failed to get info of file", res->iResponseErrorCode);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Icmd: Failed to get info of file [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }
    else if(res->iResponseFlags == BACKUP_RESPONSE)
    {
        printf(YEL"Corresponding Storage Server is down. Trying to get info from backup server\n"reset);
        fprintf(Clientlog, "[+]Icmd: Corresponding Storage Server is down. Trying to get info from backup server [Time Stamp: %f]\n", GetCurrTime(Clock));

        // Modify the path to the backup path
        memset(req->sRequestPath, 0, sizeof(req->sRequestPath));
        snprintf(req->sRequestPath, MAX_BUFFER_SIZE, "./backup%s", path);
    }

    // The response data is the IP and Port of the storage server serving the file seperated by a space
    char* ip = strtok(res->sResponseData, " ");
    char* port = strtok(NULL, " ");

    // Check if  IP and Port are valid
    if(CheckNull(ip, ErrorMsg("Invalid IP received from server", CMD_ERROR_INVALID_RECV_VALUE)))
    {
        fprintf(Clientlog, "[-]Icmd: Invalid IP received from server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }
    else if(CheckNull(port, ErrorMsg("Invalid Port received from server", CMD_ERROR_INVALID_RECV_VALUE)))
    {
        fprintf(Clientlog, "[-]Icmd: Invalid Port received from server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Connect to the storage server
    int StorageSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(CheckError(StorageSockfd, ErrorMsg("Failed to create socket", CMD_ERROR_SOCKET_FAILED)))
    {
        fprintf(Clientlog, "[-]Icmd: Failed to create socket [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    struct sockaddr_in StorageServer;
    memset(&StorageServer, 0, sizeof(StorageServer));
    StorageServer.sin_family = AF_INET;
    StorageServer.sin_addr.s_addr = inet_addr(ip);
    StorageServer.sin_port = htons(atoi(port));
    memset(StorageServer.sin_zero, '\0', sizeof(StorageServer.sin_zero));

    int iConnectStatus = connect(StorageSockfd, (struct sockaddr *)&StorageServer, sizeof(StorageServer));
    if(CheckError(iConnectStatus, ErrorMsg("Failed to connect to storage server", CMD_ERROR_CONNECT_FAILED)))
    {
        fprintf(Clientlog, "[-]Icmd: Failed to connect to storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Send the request to the storage server
    iBytesSent = send(StorageSockfd, req, sizeof(REQUEST_STRUCT), 0);
    if(iBytesSent != sizeof(REQUEST_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to send request to storage server", CMD_ERROR_SEND_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Icmd: Failed to send request to storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Recieve the Confirmation from the server
    iBytesRecv = recv(StorageSockfd, res, sizeof(RESPONSE_STRUCT), 0);
    if(iBytesRecv != sizeof(RESPONSE_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to receive confirmation from storage server", CMD_ERROR_RECV_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Icmd: Failed to receive confirmation from storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }
    else if(res->iResponseFlags == RESPONSE_FLAG_FAILURE)
    {
        char* Msg = ErrorMsg("Failed to get info of file", res->iResponseErrorCode);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Icmd: Failed to get info of file [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Receive Information About Path From Server
    PATH_INFO_STRUCT path_info_struct;
    PATH_INFO_STRUCT* path_info = &path_info_struct;
    memset(path_info, 0, sizeof(PATH_INFO_STRUCT));

    iBytesRecv = recv(StorageSockfd, path_info, sizeof(PATH_INFO_STRUCT), 0);
    if(iBytesRecv != sizeof(PATH_INFO_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to receive path info from storage server", CMD_ERROR_RECV_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Icmd: Failed to receive path info from storage server [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Extract the information from the path_info struct
    // Convert the permission to corresponding string
    char permission[10];
    memset(permission, 0, 10);
    permission[0] = (path_info->iPathPermission & S_IRUSR) ? 'r' : '-';
    permission[1] = (path_info->iPathPermission & S_IWUSR) ? 'w' : '-';
    permission[2] = (path_info->iPathPermission & S_IXUSR) ? 'x' : '-';
    permission[3] = (path_info->iPathPermission & S_IRGRP) ? 'r' : '-';
    permission[4] = (path_info->iPathPermission & S_IWGRP) ? 'w' : '-';
    permission[5] = (path_info->iPathPermission & S_IXGRP) ? 'x' : '-';
    permission[6] = (path_info->iPathPermission & S_IROTH) ? 'r' : '-';
    permission[7] = (path_info->iPathPermission & S_IWOTH) ? 'w' : '-';
    permission[8] = (path_info->iPathPermission & S_IXOTH) ? 'x' : '-';
    permission[9] = '\0';
    

    // Convert the type to corresponding string
    char type[20];
    memset(type, 0, 10);
    switch (path_info->iPathType)
    {
    case __S_IFBLK: strcat(type, "Block Device"); break;
    case __S_IFCHR: strcat(type, "Character Device"); break;
    case __S_IFDIR: strcat(type, "Directory"); break;
    case __S_IFIFO: strcat(type, "FIFO/PIPE"); break;
    case __S_IFLNK: strcat(type, "Symbolic Link"); break;
    case __S_IFREG: strcat(type, "Regular File"); break;
    case __S_IFSOCK: strcat(type, "Socket"); break;    
    default: strcat(type, "Unknown"); break;
    }

    // Convert the time to human readable format
    char ctime[50];
    memset(ctime, 0, 50);
    struct tm* tm_info;
    time_t time = path_info->iPathCreationTime;
    tm_info = localtime(&time);
    strftime(ctime, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    char mtime[50];
    memset(mtime, 0, 50);
    time = path_info->iPathModificationTime;
    tm_info = localtime(&time);
    strftime(mtime, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    char atime[50];
    memset(atime, 0, 50);
    time = path_info->iPathAccessTime;
    tm_info = localtime(&time);
    strftime(atime, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    printf("--------------------------------------------------\n"BWHT);
    printf("Path: %s\n", path_info->sPath);
    printf("Type: %s\n", type);
    printf("Size: %d Bytes\n", path_info->iPathSize);
    printf("Permission: %s (%d)\n", permission, path_info->iPathPermission);
    printf("Creation Time: %s\n", ctime);
    printf("Modification Time: %s\n", mtime);
    printf("Access Time: %s\n", atime);
    printf("Number of Links: %d\n", path_info->iPathLinks);
    printf(reset"--------------------------------------------------\n");

    fprintf(Clientlog, "[+]Icmd: Path Information:\nPath: %s\nType: %s\nSize: %d Bytes\nPermission: %d (%s)\nCreation Time: %s\nModification Time: %s [Time Stamp: %f]\n", path_info->sPath, path_info->iPathType == 0 ? "File" :path_info->iPathType == 1? "Folder": "Executable", path_info->iPathSize, path_info->iPathPermission, permission, ctime, mtime, GetCurrTime(Clock));
      
    return;
}