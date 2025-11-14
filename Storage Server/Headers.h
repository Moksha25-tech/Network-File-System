#ifndef __HEADERS_H__
#define __HEADERS_H__

#include <stdio.h>
#include "./Trie.h"

# define MAX_CONN_Q 5
#define LOG_FLUSH_INTERVAL 10


// structure for client object
typedef struct Client
{
    int socket;
    char* IP;
    int port;
}Client;


// structure for clock object
typedef struct Clock
{
    double bootTime;
    struct timespec Btime;
}CLOCK;

CLOCK* InitClock();
double GetCurrTime(CLOCK* clock);

// Trie* File_Trie;
// int NS_Write_Socket;

extern FILE* Log_File;
extern CLOCK* Clock;

void* NS_Listner_Thread(void* arg);
void* Client_Listner_Thread(void* arg);
void* Client_Handler_Thread(void* arg);



// Populates the File_Trie with the contents of the cwd
Trie* Initialize_File_Trie();

#endif // __HEADERS_H__