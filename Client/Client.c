// Standard Header Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <poll.h>
#include <signal.h>

// Custom Header Files
#include "../Externals.h"
#include "../colour.h"
#include "./Headers.h"
#include "./Hash.h"
#include "./ErrorCodes.h"

FILE *Clientlog;
HashTable *table;
unsigned long iClientID;
CLOCK *Clock;

volatile sig_atomic_t signal_received = 0;
// Set the signal flag
void set_signal(){ signal_received = 1;}

/**
 * @brief Initializes the clock object.
 * @return: A pointer to the clock object on success, NULL on failure.
 **/
CLOCK *InitClock()
{
    CLOCK *C = (CLOCK *)malloc(sizeof(CLOCK));
    if (CheckNull(C, "[-]InitClock: Error in allocating memory"))
    {
        fprintf(Clientlog, "[-]InitClock: Error in allocating memory \n");
        exit(EXIT_FAILURE);
    }

    C->bootTime = 0;
    C->bootTime = GetCurrTime(C);
    if (CheckError(C->bootTime, "[-]InitClock: Error in getting current time"))
    {
        fprintf(Clientlog, "[-]InitClock: Error in getting current time\n");
        free(C);
        exit(EXIT_FAILURE);
    }

    int err = clock_gettime(CLOCK_MONOTONIC_RAW, &C->Btime);
    if (CheckError(err, "[-]InitClock: Error in getting current time"))
    {
        fprintf(Clientlog, "[-]InitClock: Error in getting current time\n");
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
        fprintf(Clientlog, "[-]GetCurrTime: Invalid clock object\n");
        return -1;
    }
    struct timespec time;
    int err = clock_gettime(CLOCK_MONOTONIC_RAW, &time);
    if (CheckError(err, "[-]GetCurrTime: Error in getting current time"))
    {
        fprintf(Clientlog, "[-]GetCurrTime: Error in getting current time\n");
        return -1;
    }
    return (time.tv_sec + time.tv_nsec * 1e-9) - (Clock->bootTime);
}

/**
 * @brief Polls the socket to check if it is online and reconnects if it is not
 * @param sockfd The socket to poll
 * @param ip The ip address of the server
 * @param port The port of the server
 * @return value of sockfd with a connection to the server
 */
int pollServer(int sockfd, char *ip, int port)
{
    fprintf(Clientlog, "[+]pollServer: Polling server [Time Stamp: %f]\n", GetCurrTime(Clock));

    // Poll the socket to check if it is writable using poll()
    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN | POLLPRI | POLLOUT | POLLERR | POLLHUP;

    int result = poll(fds, 1, POLL_TIMEOUT);

    if (result == -1)
    {
        printf(RED "[-]pollServer: Error in poll\n" reset);
        perror("poll");
        fprintf(Clientlog, "[-]pollServer: Error in poll [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    else if (result == 0)
    {
        // Timeout occurred, socket is not writable
        // close the socket and open a new connection
        close(sockfd);

        struct sockaddr_in server_address;
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(port);
        server_address.sin_addr.s_addr = inet_addr(ip);
        memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (CheckError(sockfd, "[-]pollServer: Error in creating socket"))
        {
            fprintf(Clientlog, "[-]Error in creating socket [Time Stamp: %f]\n", GetCurrTime(Clock));
            exit(EXIT_FAILURE);
        }

        int iConnectionStatus;
        while (CheckError(iConnectionStatus = connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)), "[-]pollServer: Error in reconnecting to server"))
        {
            printf(RED "Retrying in %d seconds...\n" reset, SLEEP_TIME);
            sleep(SLEEP_TIME);
        }

        // Connection established, receive identification ID (Client ID)
        int iRecvStatus;
        printf("[+]Getting ID from Server\n");
        do
        {
            sleep(SLEEP_TIME);
            printf("[-]recv: Error in receiving Client ID.Retrying\n");
            iRecvStatus = recv(sockfd, &iClientID, sizeof(int), 0);
            if (iRecvStatus == 0)
            {
                printf(RED "[-]Client: Connection to server failed\n" reset);
                fprintf(Clientlog, "[-]Client: Connection to server failed [Time Stamp: %f]\n", GetCurrTime(Clock));
                exit(EXIT_FAILURE);
            }
        } while (CheckError(iRecvStatus, "[-]Error in receiving Client ID"));

        printf(GRN "[+]pollServer: Reconnected to the server with ID-%lu\n" reset, iClientID);
        fprintf(Clientlog, "[+]pollServer: Reconnected to the server with ID-%lu [Time Stamp: %f]\n", iClientID, GetCurrTime(Clock));
    }
    else
    {
        // Socket is writable
        fprintf(Clientlog, "[+]pollServer: Server is online [Time Stamp: %f]\n", GetCurrTime(Clock));
    }

    return sockfd;
}

/**
 * @brief Prints the prompt for the client for Interrupt handling
 * @return void
*/
void INThandler()
{
    char c;

    // Ignore the signal SIGINT
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);

    printf("\nOuch, did you hit Ctrl-C?\nDo you really want to quit? [y/n] ");
    scanf("%c", &c);
    if (c == 'y' || c == 'Y')
    {
        printf(RED "[-]Client: Exiting\n" reset);
        fprintf(Clientlog, "[-]Client: Exiting [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(1);
    }
    printf(GRN "[+]Continuing...\n" reset);
    fprintf(Clientlog, "[+]Continuing... [Time Stamp: %f]\n", GetCurrTime(Clock));

    signal_received = 0;

    act.sa_handler = set_signal;
    sigaction(SIGINT, &act, NULL);

    // Clear the input buffer
    while ((getchar()) != '\n');
    return;
}

/**
 * @brief Checks if the input is sanitized 
 * @param cInput The input to be sanitized
 * @return 0 if the input is sanitized, 1 otherwise
 * @note This function checks for null, empty string, null character, newline character
*/
int sanitize(char *cInput)
{
    if (cInput == NULL)
    {
        fprintf(Clientlog, "[-]sanitize: Null input\n");
        return 1;
    }
    if (strlen(cInput) == 0)
    {
        fprintf(Clientlog, "[-]sanitize: Empty input\n");
        return 1;
    }
    if (cInput[0] == '\0')
    {
        fprintf(Clientlog, "[-]sanitize: Null character\n");
        return 1;
    }   
    if(cInput[0] == '\n')
    {
        fprintf(Clientlog, "[-]sanitize: Newline character\n");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    // Initialize the client Setup the client CLI
    table = createHashTable(FUNCTION_COUNT);

    // Client Side Commands
    insert(table, &Hcmd, "HELP");
    insert(table, &CScmd, "CLEAR");
    insert(table, &Ecmd, "EXIT");
    // Direct Connection Commands
    insert(table, &Rcmd, "READ");
    insert(table, &Wcmd, "WRITE");
    insert(table, &Icmd, "INFO");
    // Server Side Commands
    insert(table, &LScmd, "LIST");
    // Indirect Connection Commands
    insert(table, &Dcmd, "DELETE");
    insert(table, &Ccmd, "CREATE");
    insert(table, &Cpycmd, "COPY");
    insert(table, &Mvcmd, "MOVE");
    insert(table, &Rncmd, "RENAME");

    // Initialize the client log
    Clientlog = fopen("Clientlog.log", "w");
    // Initialize the clock
    Clock = InitClock();

    // Register the signal handler
    struct sigaction act;
    act.sa_handler = set_signal;
    sigaction(SIGINT, &act, NULL);

    printf(GRN "[+]Client Initialized\n" reset);
    fprintf(Clientlog, "[+]Client Initialized [Time Stamp: %f]\n", GetCurrTime(Clock));

    // Create a socket
    int iClientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (CheckError(iClientSocket, "[-]Error in creating socket"))
    {
        fprintf(Clientlog, "[-]Error in creating socket [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    // Specify an address for the socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(NS_CLIENT_PORT);
    server_address.sin_addr.s_addr = inet_addr(NS_IP);

    // Connect to the server
    int iConnectionStatus;
    while (CheckError(iConnectionStatus = connect(iClientSocket, (struct sockaddr *)&server_address, sizeof(server_address)), "[-]Error in connecting to server"))
    {
        printf(RED "Retrying...\n" reset);
        sleep(1);
    }

    printf("[+]Connected to the server\n");
    fprintf(Clientlog, "[+]Connected to the server [Time Stamp: %f]\n", GetCurrTime(Clock));

    // Connection established, receive identification ID (Client ID)
    int iRecvStatus = recv(iClientSocket, &iClientID, sizeof(unsigned long), 0);
    printf("[+]Getting ID from Server\n");

    if (CheckError(iRecvStatus, "[-]recv: Error in receiving Client ID"))
    {
        fprintf(Clientlog, "[-]recv: Error in receiving Client ID [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }
    else if (iRecvStatus == 0)
    {
        printf(RED "[-]Client: Connection to server failed\n" reset);
        fprintf(Clientlog, "[-]Client: Connection to server failed [Time Stamp: %f]\n", GetCurrTime(Clock));
        exit(EXIT_FAILURE);
    }

    printf(GRN "[+]Connected to the server. Connection ID: %lu\n" reset, iClientID);
    fprintf(Clientlog, "[+]Connected to the server. Connection ID: %lu [Time Stamp: %f]\n", iClientID, GetCurrTime(Clock));
    printf(YEL "[+]Press enter to continue..." reset);
    getchar();

    clearScreen();

    // Setup the input command system for the client

    while (iClientSocket = pollServer(iClientSocket, NS_IP, NS_CLIENT_PORT))
    {
        if (signal_received) INThandler();

        prompt();
        // Get input from the user
        char cInput[INPUT_SIZE];
        scanf("%[^\n]%*c", cInput);

        // Sanitize the input (check for null , empty string, Ctrl+C, etc)
        if(sanitize(cInput)) continue;        

        // Parse the input
        char *cCommand = strtok(cInput, " ");
        char *cArgs = strtok(NULL, "\0");

        // Execute the command
        functionPointer CMD = lookup(table, cCommand);
        char *Msg = ErrorMsg("Command not found", CMD_ERROR_INVALID_COMMAND);
        if (CheckNull(CMD, Msg))
        {
            fprintf(Clientlog, "[-]Command not found [Time Stamp: %f]\n", GetCurrTime(Clock));
            continue;
        }

        CMD(cArgs, iClientSocket);
        fflush(Clientlog);
    }

    return 0;
}