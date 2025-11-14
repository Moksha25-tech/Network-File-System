#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include "Headers.h"
#include "Client_Handle.h"
#include "../colour.h"

/**
 * @brief Gets the ID of the client.
 * @param clientHandle The client handle of the client.
 * @return The client ID.
 * @note called while adding a client to the client list.
*/
unsigned long GetClientID(CLIENT_HANDLE_STRUCT *clientHandle)
{
    // Simple Hash ID from Ip and Port by concatenating them in a single integer
    struct in_addr ip;
    inet_aton(clientHandle->sClientIP, &ip);    
    int port = clientHandle->sClientPort;

    unsigned long clientID = ((uint64_t)ntohl(ip.s_addr) << IP_LENGTH) | port;

    return clientID;    
}

CLIENT_HANDLE_LIST_STRUCT* InitializeClientHandleList()
{
    CLIENT_HANDLE_LIST_STRUCT *clientHandleList = (CLIENT_HANDLE_LIST_STRUCT *) malloc(sizeof(CLIENT_HANDLE_LIST_STRUCT));
    memset(clientHandleList, 0, sizeof(clientHandleList));
    pthread_mutex_init(&clientHandleList->clientListMutex, NULL);
    return clientHandleList;
}

/**
 * Adds a client to the client list.
 *
 * @param clientHandle The client handle of the client to be added.
 * @return -1 if the maximum number of clients have been reached, otherwise 0.
 * @note The client handle is modified to include the client ID.
 */
int AddClient(CLIENT_HANDLE_STRUCT *clientHandle, CLIENT_HANDLE_LIST_STRUCT *clientHandleList)
{
    clientHandle->ClientID = -1;
    pthread_mutex_lock(&clientHandleList->clientListMutex);
    if(clientHandleList->iClientCount == MAX_CLIENTS)
    {
        pthread_mutex_unlock(&clientHandleList->clientListMutex);
        printf(RED"[-]AddClient: Maximum number of clients reached\n"reset);
        fprintf(logs, "[-]AddClient: Maximum number of clients reache [Time Stamp: %f]\n", GetCurrTime(Clock));
        return -1;
    }
    
    // Find First Empty Slot
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clientHandleList->InUseList[i] == 0)
        {
            clientHandle->ClientID = GetClientID(clientHandle);
            clientHandleList->InUseList[i] = 1;
            clientHandleList->clientList[i] = *clientHandle;
            clientHandleList->iClientCount++;
            pthread_mutex_unlock(&clientHandleList->clientListMutex);
            printf(GRN"[+]AddClient: Client-%lu (%s:%d) added to client list\n"reset, clientHandle->ClientID, clientHandle->sClientIP, clientHandle->sClientPort);
            fprintf(logs, "[+]AddClient: Client-%lu (%s:%d) added to client list [Time Stamp: %f]\n", clientHandle->ClientID, clientHandle->sClientIP, clientHandle->sClientPort, GetCurrTime(Clock));
            return 0;
        }
    }

    pthread_mutex_unlock(&clientHandleList->clientListMutex);
    printf(RED"[-]AddClient: Error adding client-%lu (%s:%d)\n"reset, clientHandle->ClientID, clientHandle->sClientIP, clientHandle->sClientPort);
    fprintf(logs, "[-]AddClient: Error adding client-%lu (%s:%d) [Time Stamp: %f]\n", clientHandle->ClientID, clientHandle->sClientIP, clientHandle->sClientPort, GetCurrTime(Clock));
    return -1;    
}
/**
 * Removes a client from the client list.
 *
 * @param ClientID The ID of the client to be removed.
 * @return -1 if there are no clients in the list or the client is not found, otherwise 0.
 */
int RemoveClient(unsigned long ClientID, CLIENT_HANDLE_LIST_STRUCT *clientHandleList)
{
    pthread_mutex_lock(&clientHandleList->clientListMutex);
    if(clientHandleList->iClientCount == 0)
    {
        pthread_mutex_unlock(&clientHandleList->clientListMutex);
        printf(RED"[-]RemoveClient: No clients to remove\n"reset);
        fprintf(logs, "[-]RemoveClient: No clients to remove [Time Stamp: %f]\n", GetCurrTime(Clock));
        return -1;
    }    

    // Find the client
    CLIENT_HANDLE_STRUCT *clientHandle = NULL;
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clientHandleList->clientList[i].ClientID == ClientID)
        {
            clientHandle = &clientHandleList->clientList[i];
            clientHandleList->InUseList[i] = 0;
            break;
        }
    }

    if(CheckNull(clientHandle, "[-]RemoveClient: Client not found"))
    {
        pthread_mutex_unlock(&clientHandleList->clientListMutex);
        fprintf(logs, "[-]RemoveClient: Client not found [Time Stamp: %f]\n", GetCurrTime(Clock));
        return -1;
    }

    // Remove the client
    clientHandleList->iClientCount--;
    pthread_mutex_unlock(&clientHandleList->clientListMutex);

    printf(GRN"[+]RemoveClient: Client-%lu (%s:%d) removed from client list\n"reset, clientHandle->ClientID, clientHandle->sClientIP, clientHandle->sClientPort);
    fprintf(logs, "[+]RemoveClient: Client-%lu (%s:%d) removed from client list [Time Stamp: %f]\n", clientHandle->ClientID, clientHandle->sClientIP, clientHandle->sClientPort, GetCurrTime(Clock));
}
/**
 * @brief Gets the client handle of the client.
 * @param ClientID The ID of the client.
 * @param clientHandleList The list of client handles.
 * @return The client handle of the client if found, otherwise NULL.
 */
CLIENT_HANDLE_STRUCT* GetClient(unsigned long ClientID, CLIENT_HANDLE_LIST_STRUCT *clientHandleList)
{
    pthread_mutex_lock(&clientHandleList->clientListMutex);
    if(clientHandleList->iClientCount == 0)
    {
        pthread_mutex_unlock(&clientHandleList->clientListMutex);
        printf(RED"[-]GetClient: No clients to get\n"reset);
        fprintf(logs, "[-]GetClient: No clients to get [Time Stamp: %f]\n", GetCurrTime(Clock));
        return NULL;
    }

    // Find the client
    CLIENT_HANDLE_STRUCT *clientHandle = NULL;
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clientHandleList->clientList[i].ClientID == ClientID)
        {
            clientHandle = &clientHandleList->clientList[i];
            break;
        }
    }

    if(CheckNull(clientHandle, "[-]GetClient: Client not found"))
    {
        pthread_mutex_unlock(&clientHandleList->clientListMutex);
        fprintf(logs, "[-]GetClient: Client not found [Time Stamp: %f]\n", GetCurrTime(Clock));
        return NULL;
    }

    pthread_mutex_unlock(&clientHandleList->clientListMutex);
    return clientHandle;
}
