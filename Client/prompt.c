#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../colour.h"
#include "./Headers.h"

/**
 * @brief Prints the prompt for the user (Username@Hostname:CWD>)
 * @note CWD = Current Working Directory
*/
void prompt()
{
    char* username = getenv("USER");
    char hostname[PROMPT_LEN];
    gethostname(hostname, PROMPT_LEN);
    char cwd[PROMPT_LEN];
    getcwd(cwd, PROMPT_LEN);

    char Prompt[PROMPT_LEN*3];
    snprintf(Prompt, sizeof(Prompt), "%s%s@%s%s:%s%s%s%s> ", BHYEL, username, hostname, reset, BHWHT, cwd, "/mount", reset);

    printf("%s", Prompt);  
}