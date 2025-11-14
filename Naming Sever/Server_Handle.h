#ifndef __SERVER_HANDLE_H__
#define __SERVER_HANDLE_H__

#include "../Externals.h"
#include <stdio.h>
#include <pthread.h>

#define MAX_SERVERS 5
#define BACKUP_SERVERS 0

typedef struct SERVER_HANDLE_STRUCT
{
    unsigned long ServerID;
    char sServerIP[IP_LENGTH];                            // IP of the storage server
    int sServerPort;                                      // Port used by the storage server to connect to the naming server                    
    int sServerPort_NServer;                              // Port on which the storage server will listen for NServer
    int sServerPort_Client;                               // Port on which the storage server will listen for client
    int sSocket_Write;                                    // Socket to write to the server
    int sSocket_Read;                                     // Socket to read from the server
    struct SERVER_HANDLE_STRUCT* backupServers[BACKUP_SERVERS];  // Array of backup servers
    // char MountPaths[MAX_BUFFER_SIZE];                  // \n separated list of mount paths

} SERVER_HANDLE_STRUCT;

typedef struct SERVER_HANDLE_LIST_STRUCT
{
    SERVER_HANDLE_STRUCT serverList[MAX_SERVERS];
    short Active[MAX_SERVERS];
    short Running[MAX_SERVERS];
    int backupServerCount[MAX_SERVERS];
    int iServerCount;
    pthread_mutex_t severListMutex;
} SERVER_HANDLE_LIST_STRUCT;


SERVER_HANDLE_LIST_STRUCT* InitializeServerHandleList();

int AddServer(SERVER_HANDLE_STRUCT *serverHandle, SERVER_HANDLE_LIST_STRUCT *serverHandleList);

int RemoveServer(unsigned long serverID, SERVER_HANDLE_LIST_STRUCT *serverHandleList);

int SetInactive(unsigned long serverID, SERVER_HANDLE_LIST_STRUCT *serverHandleList);

int SetActive(unsigned long serverID, SERVER_HANDLE_LIST_STRUCT *serverHandleList);

int AssignBackupServer(SERVER_HANDLE_LIST_STRUCT *serverHandleList, unsigned long serverID);

unsigned long GetServerID(SERVER_HANDLE_STRUCT *serverHandle); 

int IsActive(unsigned long serverID, SERVER_HANDLE_LIST_STRUCT *serverHandleList);

SERVER_HANDLE_STRUCT* GetActiveBackUp(SERVER_HANDLE_LIST_STRUCT *serverHandleList, SERVER_HANDLE_STRUCT* BackUpList[]);

#endif