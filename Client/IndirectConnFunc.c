#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr

// Custom Header Files
#include "../Externals.h"
#include "../colour.h"
#include "./Headers.h"
#include "./Hash.h"
#include "./ErrorCodes.h"

void LScmd(char* arg, int ServerSockfd)
{
    if(CheckNull(arg, ErrorMsg("NULL Argument\nUSAGE: LIST <Path>", CMD_ERROR_INVALID_ARGUMENTS)))
    {
        printf(BWHT"USE 'LIST mount' or 'LIST . to list entire directory tree\n"reset);
        fprintf(Clientlog, "[-]LScmd: Invalid Argument [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    arg = strtok(arg, " \t\n");
    if(strtok(NULL, " \t\n") != NULL)
    {
        fprintf(Clientlog, "[-]LScmd: Invalid Argument Count [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    char* path = arg;
    fprintf(Clientlog, "[+]LScmd: Listing Path %s [Time Stamp: %f]\n", path, GetCurrTime(Clock));

    REQUEST_STRUCT req_struct;
    REQUEST_STRUCT* req = &req_struct;
    memset(req, 0, sizeof(REQUEST_STRUCT));

    req->iRequestOperation = CMD_LIST;
    req->iRequestClientID = iClientID;
    strncpy(req->sRequestPath, path, MAX_BUFFER_SIZE);
    // req->iRequestFlags = 0;

    int iBytesSent = send(ServerSockfd, req, sizeof(REQUEST_STRUCT), 0);

    if(iBytesSent != sizeof(REQUEST_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to send request to server", CMD_ERROR_SEND_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]LScmd: Failed to send request [Time Stamp: %f]\n", GetCurrTime(Clock));
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
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]LScmd: Failed to receive response [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    if(res->iResponseFlags != RESPONSE_FLAG_SUCCESS)
    {
        char* Msg = ErrorMsg(res->sResponseData, res->iResponseErrorCode);
        printf(RED"%s\n"reset, Msg);
        
        fprintf(Clientlog, "%s [Time Stamp: %f]\n", Msg, GetCurrTime(Clock));
        free(Msg);
        return;
    }

    printf(GRN"%s\n"reset, res->sResponseData);
    fprintf(Clientlog, "[+]LScmd: Successfully listed directory [Time Stamp: %f]\n", GetCurrTime(Clock));
    return;
}
void Cpycmd(char* arg, int ServerSockfd)
{
    return;
}
void Mvcmd(char* arg, int ServerSockfd)
{
    return;
}
void Dcmd(char* arg, int ServerSockfd)
{
    return;
}
void Ccmd(char* arg, int ServerSockfd)
{
    
    return;
}
void Rncmd(char* arg, int ServerSockfd)
{
    // Check if the argument is NULL
    if(CheckNull(arg, ErrorMsg("NULL Argument\nUSAGE: RENAME <Source Path> <Target Name>", CMD_ERROR_INVALID_ARGUMENTS)))
    {
        fprintf(Clientlog, "[-]Rncmd: Invalid Argument [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Tokenize the argument
    char* src = strtok(arg, " \t\n");
    char* target = strtok(NULL, " \t\n");

    // Check if the argument count is correct
    if(CheckNull(target, ErrorMsg("Invalid Argument Count\nUSAGE: RENAME <Source Path> <Target Name>", CMD_ERROR_INVALID_ARGUMENTS)))
    {
        fprintf(Clientlog, "[-]Rncmd: Invalid Argument Count [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }

    // Check if the argument count is correct
    if(strtok(NULL, " \t\n") != NULL)
    {
        printf(RED"Invalid Argument Count\nUSAGE: RENAME <Source Path> <Target Name>\n"reset);
        fprintf(Clientlog, "[-]Rncmd: Invalid Argument Count [Time Stamp: %f]\n", GetCurrTime(Clock));
        return;
    }


    // Log the command
    fprintf(Clientlog, "[+]Rncmd: Renaming %s to %s [Time Stamp: %f]\n", src, target, GetCurrTime(Clock));

    // Create a request struct
    REQUEST_STRUCT req_struct;
    REQUEST_STRUCT* req = &req_struct;
    memset(req, 0, sizeof(REQUEST_STRUCT));

    // Fill the request struct
    req->iRequestOperation = CMD_RENAME;
    req->iRequestClientID = iClientID;
    snprintf(req->sRequestPath, MAX_BUFFER_SIZE, "%s %s", src, target);

    // Send the request to the server
    int iBytesSent = send(ServerSockfd, req, sizeof(REQUEST_STRUCT), 0);
    if(iBytesSent != sizeof(REQUEST_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to send request to server", CMD_ERROR_SEND_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Rncmd: Failed to send request [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Receive the Processing confirmation from the server
    RESPONSE_STRUCT res_struct;
    RESPONSE_STRUCT* res = &res_struct;
    memset(res, 0, sizeof(RESPONSE_STRUCT));

    int iBytesRecv = recv(ServerSockfd, res, sizeof(RESPONSE_STRUCT), 0);
    if(iBytesRecv != sizeof(RESPONSE_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to receive response from server", CMD_ERROR_RECV_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Rncmd: Failed to receive response [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Check if the operation was successful
    if(res->iResponseFlags != RESPONSE_FLAG_SUCCESS)
    {
        char* Msg = ErrorMsg(res->sResponseData, res->iResponseErrorCode);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Rncmd: Failed to rename file [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Receive the ACK from the server
    ACK_STRUCT ack_struct;
    ACK_STRUCT* ack = &ack_struct;
    memset(ack, 0, sizeof(ACK_STRUCT));

    iBytesRecv = recv(ServerSockfd, ack, sizeof(ACK_STRUCT), 0);
    if(iBytesRecv != sizeof(ACK_STRUCT))
    {
        char* Msg = ErrorMsg("Failed to receive response from server", CMD_ERROR_RECV_FAILED);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Rncmd: Failed to receive response [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Log the ACK
    fprintf(Clientlog, "[+]Rncmd: Received ACK with data %s [Time Stamp: %f]\n", ack->sAckData, GetCurrTime(Clock));

    // Check if the operation was successful
    if(ack->iAckFlags == ACK_FLAG_SUCCESS)
    {
        char* Msg = ErrorMsg("Failed to rename file", ack->iAckErrorCode);
        printf(RED"%s\n"reset, Msg);
        fprintf(Clientlog, "[-]Rncmd: Failed to rename file [Time Stamp: %f]\n", GetCurrTime(Clock));
        free(Msg);
        return;
    }

    // Print the success message
    printf(GRN"%s\n"reset, ack->sAckData);
    fprintf(Clientlog, "[+]Rncmd: Successfully renamed file [Time Stamp: %f]\n", GetCurrTime(Clock));
    
    return;
}
