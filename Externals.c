#include "./Externals.h"
#include "./colour.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>

jmp_buf jmpbuffer;

/**
 * Checks if the status is less than 0 and prints the error message.
 *
 * @param iStatus The status to be checked.
 * @param sErrorMsg The error message to be printed.
 * @return 1 if the status is less than 0, otherwise 0
 * @note The error message is freed in the CheckError function (if allocated)
 */
int CheckError(int iStatus, char *sErrorMsg)
{
    if(iStatus < 0)
    {
        printf("%s\n",sErrorMsg);
        if(setjmp(jmpbuffer) != 0)
            free(sErrorMsg);
        return 1;
    }
    else if(setjmp(jmpbuffer) != 0) 
    {
       free(sErrorMsg);
    }
    return 0;
}

/**
 * Checks if the pointer is NULL and prints the error message.
 *
 * @param ptr The pointer to be checked.
 * @param sErrorMsg The error message to be printed.
 * @return 1 if the pointer is NULL, otherwise 0.
 */
int CheckNull(void *ptr, char *sErrorMsg)
{
    if(ptr == NULL)
    {
        printf("%s\n",sErrorMsg);
        return 1;
    }
    if(setjmp(jmpbuffer) != 0)
    {
       free(sErrorMsg);
    }
    return 0;
}

/**
 * @brief Builds an error message with the given error code and message.
 * @param msg The message to be printed.
 * @param ErrorCode The error code to be printed.
 * @return The error message.
 * @note The returned error message is freed in the CheckError function.
*/
char* ErrorMsg(char* msg, int ErrorCode)
{
    char* ErrorMsg = (char*)malloc(sizeof(char) * ERROR_MSG_LEN);
    if (ErrorMsg == NULL)
    {
        printf(RED"[-]ErrorMsg: Error in malloc\n"reset);
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    snprintf(ErrorMsg, ERROR_MSG_LEN, RED"ERROR: %d-%s"reset, ErrorCode, msg);
    return ErrorMsg;
}