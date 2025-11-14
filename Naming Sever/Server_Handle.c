#include "Server_Handle.h"
#include "./Headers.h"
#include "../colour.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <limits.h>

/**
 * @brief Gets the server ID
 * @param serverHandle: The server handle object
 * @return: The server ID
 * @note: called while adding a server to the server list
*/
unsigned long GetServerID(SERVER_HANDLE_STRUCT *serverHandle)
{
    // Simple Hash ID from Ip and Port by concatenating them in a single integer
    struct in_addr ip;
    inet_aton(serverHandle->sServerIP, &ip);    
    int port = serverHandle->sServerPort;

    unsigned long serverID = ((uint64_t)ntohl(ip.s_addr) << IP_LENGTH) | port;

    return serverID;    
}

/**
 * @brief Initializes the Server Handle List
 * @return: The Server Handle List object
*/
SERVER_HANDLE_LIST_STRUCT* InitializeServerHandleList()
{
    SERVER_HANDLE_LIST_STRUCT *serverHandleList = (SERVER_HANDLE_LIST_STRUCT *)malloc(sizeof(SERVER_HANDLE_LIST_STRUCT));
    memset(serverHandleList, 0, sizeof(serverHandleList));
    pthread_mutex_init(&serverHandleList->severListMutex, NULL);
    return serverHandleList;
}
/**
 * @brief Adds a server to the Server Handle List
 * @param serverHandle: The server handle object
 * @param serverHandleList: The server handle list object
 * @return: 0 on success, -1 on failure
 * @note: The server handle object is modified to include the server ID
 * @note: If a previous server with the same ID is present, it is set to active
*/
int AddServer(SERVER_HANDLE_STRUCT *serverHandle, SERVER_HANDLE_LIST_STRUCT *serverHandleList)
{
    pthread_mutex_lock(&serverHandleList->severListMutex);
    if (serverHandleList->iServerCount >= MAX_SERVERS)
    {
        printf(RED "[-]AddServer: Server Handle List is full\n" reset);
        fprintf(logs, "[-]AddServer: Server Handle List is full\n");
        pthread_mutex_unlock(&serverHandleList->severListMutex);
        return -1;
    }

    // Find the first empty slot
    for(int i = 0; i < MAX_SERVERS; i++)
    {
        if (serverHandleList->Active[i] == 0)
        {
            serverHandle->ServerID = GetServerID(serverHandle);
            serverHandleList->serverList[i] = *serverHandle;
            serverHandleList->Active[i] = 1;
            serverHandleList->Running[i] = 1;
            serverHandleList->iServerCount++;
            printf(GRN "[+]AddServer: Added server %lu (%s:%d)\n" reset, serverHandle->ServerID, serverHandle->sServerIP, serverHandle->sServerPort);
            fprintf(logs, "[+]AddServer: Added server %lu (%s:%d)\n", serverHandle->ServerID, serverHandle->sServerIP, serverHandle->sServerPort);
            pthread_mutex_unlock(&serverHandleList->severListMutex);
            return 0;
        }
        else
        {
            // If a server with the same ID is present, set it to active
            if(serverHandleList->serverList[i].ServerID == serverHandle->ServerID)
            {
                serverHandleList->Active[i] = 1;
                serverHandleList->Running[i] = 1;
                printf(GRN "[+]AddServer: Server %ld (%s:%d) reconnected, set to active\n" reset, serverHandle->ServerID, serverHandle->sServerIP, serverHandle->sServerPort);
                fprintf(logs, "[+]AddServer: Server %ld (%s:%d) reconnected, set to active\n", serverHandle->ServerID, serverHandle->sServerIP, serverHandle->sServerPort);
                pthread_mutex_unlock(&serverHandleList->severListMutex);
                return 0;
            }
        }
    }
    pthread_mutex_unlock(&serverHandleList->severListMutex);
    printf(RED "[-]AddServer: Error adding server %lu (%s:%d)\n" reset, serverHandle->ServerID, serverHandle->sServerIP, serverHandle->sServerPort);
    fprintf(logs, "[-]AddServer: Error adding server %lu (%s:%d)\n", serverHandle->ServerID, serverHandle->sServerIP, serverHandle->sServerPort);
    return -1;
}
/**'
 * @brief Removes a server from the Server Handle List
 * @param serverID: The server ID
 * @param serverHandleList: The server handle list object
 * @return: 0 on success, -1 on failure
*/
int RemoveServer(unsigned long serverID, SERVER_HANDLE_LIST_STRUCT *serverHandleList)
{
    pthread_mutex_lock(&serverHandleList->severListMutex);
    // Find the server
    for(int i = 0; i < MAX_SERVERS; i++)
    {
        if(serverHandleList->Running[i] == 1 && serverHandleList->serverList[i].ServerID == serverID)
        {
            serverHandleList->Active[i] = 0;
            serverHandleList->Running[i] = 0;
            serverHandleList->iServerCount--;
            pthread_mutex_unlock(&serverHandleList->severListMutex);
            printf(GRN "[+]RemoveServer: Removed server %lu (%s:%d) from ServerHandleList\n" reset, serverHandleList->serverList[i].ServerID, serverHandleList->serverList[i].sServerIP, serverHandleList->serverList[i].sServerPort);
            fprintf(logs, "[+]RemoveServer: Removed server %lu (%s:%d) from ServerHandleList\n", serverHandleList->serverList[i].ServerID, serverHandleList->serverList[i].sServerIP, serverHandleList->serverList[i].sServerPort);
            return 0;
        }
    }
    pthread_mutex_unlock(&serverHandleList->severListMutex);
    printf(RED "[-]RemoveServer: Server-%lu not in ServerHandleList \n" reset, serverID);
    fprintf(logs, "[-]RemoveServer: Server-%lu not in ServerHandleList \n", serverID);
    return -1;
}
/**
 * @brief Sets a server to inactive
 * @param serverID: The server ID
 * @param serverHandleList: The server handle list object
 * @return: 0 on success, -1 on failure
*/
int SetInactive(unsigned long serverID, SERVER_HANDLE_LIST_STRUCT *serverHandleList)
{
    // Find the server and set it to inactive
    for(int i = 0; i < MAX_SERVERS; i++)
    {
        if(serverHandleList->Active[i] == 1 && serverHandleList->serverList[i].ServerID == serverID)
        {
            serverHandleList->Running[i] = 0;
            printf(GRN "[+]SetInactive: Set server %lu (%s:%d) to inactive\n" reset, serverHandleList->serverList[i].ServerID, serverHandleList->serverList[i].sServerIP, serverHandleList->serverList[i].sServerPort);
            fprintf(logs, "[+]SetInactive: Set server %lu (%s:%d) to inactive\n", serverHandleList->serverList[i].ServerID, serverHandleList->serverList[i].sServerIP, serverHandleList->serverList[i].sServerPort);
            return 0;
        }
    }
    printf(RED "[-]SetInactive: Server-%lu not in ServerHandleList \n" reset, serverID);
    fprintf(logs, "[-]SetInactive: Server-%lu not in ServerHandleList \n", serverID);
    return -1;
}
/**
 * @brief Sets a server to active
 * @param serverID: The server ID
 * @param serverHandleList: The server handle list object
 * @return: 0 on success, -1 on failure
 * @note: if server is already active (running) return with success
*/
int SetActive(unsigned long serverID, SERVER_HANDLE_LIST_STRUCT *serverHandleList)
{
    // Find the server and set it to active
    pthread_mutex_lock(&serverHandleList->severListMutex);
    for(int i = 0; i < MAX_SERVERS; i++)
    {
        if(serverHandleList->Active[i] == 1 && serverHandleList->serverList[i].ServerID == serverID)
        {
            serverHandleList->Running[i] = 1;
            pthread_mutex_unlock(&serverHandleList->severListMutex);
            printf(GRN "[+]SetActive: Set server %lu (%s:%d) to active\n" reset, serverHandleList->serverList[i].ServerID, serverHandleList->serverList[i].sServerIP, serverHandleList->serverList[i].sServerPort);
            fprintf(logs, "[+]SetActive: Set server %lu (%s:%d) to active\n", serverHandleList->serverList[i].ServerID, serverHandleList->serverList[i].sServerIP, serverHandleList->serverList[i].sServerPort);
            return 0;
        }
    }
    pthread_mutex_unlock(&serverHandleList->severListMutex);
    printf(RED "[-]SetActive: Server-%lu not in ServerHandleList \n" reset, serverID);
    fprintf(logs, "[-]SetActive: Server-%lu not in ServerHandleList \n", serverID);
    return -1;
}
/**
 * @brief Assigns backup servers to a server with given ID
 * @param serverHandleList: The server handle list object
 * @param serverID: The server ID
 * @return: 0 on success, -1 on failure
 * @note: The backup servers assigned on basis of minimum number of active backups
 * @note: for severs with preassigned backup servers, the backup servers are not changed (return with SUCESS)
*/
int AssignBackupServer(SERVER_HANDLE_LIST_STRUCT *serverHandleList, unsigned long serverID)
{
    // find the server
    SERVER_HANDLE_STRUCT *serverHandle = NULL;
    for(int i = 0; i < MAX_SERVERS; i++)
    {
        if( (serverHandleList->Active[i] == 1) && (serverHandleList->serverList[i].ServerID == serverID) )
        {
            serverHandle = &serverHandleList->serverList[i];
            break;
        }
    }
    if(serverHandle == NULL)
    {
        printf(RED "[-]AssignBackupServer: Server-%lu not in ServerHandleList \n" reset, serverID);
        fprintf(logs, "[-]AssignBackupServer: Server-%lu not in ServerHandleList \n", serverID);
        return -1;
    }
    else if(serverHandle->backupServers[0] != NULL)
    {
        printf(GRN "[+]AssignBackupServer: Server %lu (%s:%d) already has backup servers\n" reset, serverHandle->ServerID, serverHandle->sServerIP, serverHandle->sServerPort);
        fprintf(logs, "[+]AssignBackupServer: Server %lu (%s:%d) already has backup servers\n", serverHandle->ServerID, serverHandle->sServerIP, serverHandle->sServerPort);
        return 0;
    }

    int BackUpCount = 0;
    int itt = 0;
    while(BackUpCount < BACKUP_SERVERS && itt < MAX_SERVERS)
    {
        // assign the backup server with the least number of active backups
        SERVER_HANDLE_STRUCT *backupServer = NULL;
        int minBackups = INT_MAX;
        for(int i = 0; i < MAX_SERVERS; i++)
        {
            // dont assign the server as its own backup            
            // assign a unique backup server (not in previous backups)            
            // find the backup server with the least number of active backups
            
            int flag = 0;

            flag = flag || (serverHandleList->Running[i] == 0);
            flag = flag || (serverHandleList->Active[i] == 0);
            flag = flag || (serverHandleList->serverList[i].ServerID != serverHandle->ServerID);

            for(int j = 0; j < BackUpCount; j++)
            {
                flag = flag || (serverHandle->backupServers[j]->ServerID != serverHandleList->serverList[i].ServerID);
            }

            flag = flag || (serverHandleList->backupServerCount[i] <= minBackups);

            if(flag)
            {
                // assign the backup server
                backupServer = &serverHandleList->serverList[i];

                serverHandle->backupServers[BackUpCount] = backupServer;
                serverHandleList->backupServerCount[i]++;
                BackUpCount++;
            }

        }

        itt++;
    }

    if(BackUpCount != BACKUP_SERVERS)
    {
        printf(RED "[-]AssignBackupServer: Error assigning backup servers for server %lu\n" reset, serverID);
        fprintf(logs, "[-]AssignBackupServer: Error assigning backup servers for server %lu\n", serverID);
        return -1;
    }

    printf(GRN "[+]AssignBackupServer: Assigned backup servers for server-%lu (%s:%d)" reset, serverHandle->ServerID, serverHandle->sServerIP, serverHandle->sServerPort);
    printf("\nBackups { ");
    for(int i = 0; i < BackUpCount; i++)
    {
        printf("%d:(%lu), ", i+1, serverHandle->backupServers[i]->ServerID);
    }
    printf(" }\n");

    fprintf(logs, "[+]AssignBackupServer: Assigned backup servers for server-%lu (%s:%d)", serverHandle->ServerID, serverHandle->sServerIP, serverHandle->sServerPort);
    fprintf(logs, "\nBackups { ");
    for(int i = 0; i < BackUpCount; i++)
    {
        fprintf(logs, "%d:(%lu), ", i+1, serverHandle->backupServers[i]->ServerID);
    }
    fprintf(logs, " }\n"); 
    
    return 0;
}

/**
 * @brief Checks if a server is active
 * @param serverID: The server ID
 * @param serverHandleList: The server handle list object
 * @return: 1 if active, 0 if inactive, -1 on failure
 * @note: if server is not present in the server handle list, return with failure
*/
int IsActive(unsigned long serverID, SERVER_HANDLE_LIST_STRUCT *serverHandleList)
{
    pthread_mutex_lock(&serverHandleList->severListMutex);
    // Find the server and return if it is active
    for(int i = 0; i < MAX_SERVERS; i++)
    {
        if(serverHandleList->Active[i] == 1 && serverHandleList->serverList[i].ServerID == serverID)
        {
            pthread_mutex_unlock(&serverHandleList->severListMutex);
            return serverHandleList->Running[i];
        }
    }
    pthread_mutex_unlock(&serverHandleList->severListMutex);
    return -1;
}

/**
 * @brief Get an running backup server from the backup server handle list
 * @param serverHandleList: The server handle list object
 * @param BackUpList: The backup server handle list object
 * @return: First Running backup server handle object or NULL if no backup server is running
 * @note: If no backup server is running, return NULL
*/
SERVER_HANDLE_STRUCT* GetActiveBackUp(SERVER_HANDLE_LIST_STRUCT *serverHandleList, SERVER_HANDLE_STRUCT * BackUpList[])
{
    for(int i = 0; i < BACKUP_SERVERS; i++)
    {
        if(IsActive(BackUpList[i]->ServerID, serverHandleList))
        {
            return BackUpList[i];
        }
    }
    return NULL;
}