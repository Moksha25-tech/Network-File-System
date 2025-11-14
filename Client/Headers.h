#ifndef HEADERS_H
#define HEADERS_H

// Standard Libraries
#include <sys/time.h>
#include <stdio.h>

// Custom Libraries
#include "./Hash.h"

#define POLL_TIMEOUT 2
#define SLEEP_TIME 5
#define FUNCTION_COUNT 127

#define PROMPT_LEN 1024

// structure for clock object
typedef struct Clock
{
    double bootTime;
    struct timespec Btime;
}CLOCK;

CLOCK* InitClock();
double GetCurrTime(CLOCK* clock);

// Constants
extern FILE* Clientlog;
extern HashTable *table;
extern unsigned long iClientID;
extern CLOCK* Clock;


// Function Prototypes
int pollServer(int sockfd, char* ip, int port);
void prompt();

//Client Side Commands
void Ecmd(char* arg, int ServerSockfd);
void Hcmd(char* arg, int ServerSockfd);
void CScmd(char* arg, int ServerSockfd);

// Direct Connection Commands
void Rcmd(char* arg, int ServerSockfd);
void Wcmd(char* arg, int ServerSockfd);
void Icmd(char* arg, int ServerSockfd);


// Server Side Commands
void LScmd(char* arg, int ServerSockfd);

// Indirect Connection Commands
void Cpycmd(char* arg, int ServerSockfd);
void Mvcmd(char* arg, int ServerSockfd);
void Dcmd(char* arg, int ServerSockfd);
void Ccmd(char* arg, int ServerSockfd);
void Rncmd(char* arg, int ServerSockfd);


#endif