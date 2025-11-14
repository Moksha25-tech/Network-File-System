#ifndef __CLIENT_HANDLE_H__
#define __CLIENT_HANDLE_H__

#include "../Externals.h"
#include <stdio.h>
#include <pthread.h>

#define MAX_CLIENTS 5

typedef struct CLIENT_HANDLE_STRUCT
{
    unsigned long ClientID;
    char sClientIP[IP_LENGTH];
    int sClientPort;
    int iClientSocket;
} CLIENT_HANDLE_STRUCT;

typedef struct CLIENT_HANDLE_LIST_STRUCT
{
    CLIENT_HANDLE_STRUCT clientList[MAX_CLIENTS];
    short InUseList[MAX_CLIENTS];
    int iClientCount;
    pthread_mutex_t clientListMutex;
} CLIENT_HANDLE_LIST_STRUCT;

// Function Prototypes
CLIENT_HANDLE_LIST_STRUCT *InitializeClientHandleList();
int AddClient(CLIENT_HANDLE_STRUCT *clientHandle, CLIENT_HANDLE_LIST_STRUCT *clientHandleList);
int RemoveClient(unsigned long clientID, CLIENT_HANDLE_LIST_STRUCT *clientHandleList);

unsigned long GetClientID(CLIENT_HANDLE_STRUCT *clientHandle);
CLIENT_HANDLE_STRUCT *GetClient(unsigned long ClientID, CLIENT_HANDLE_LIST_STRUCT *clientHandleList);

#endif