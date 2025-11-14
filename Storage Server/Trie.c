#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./Trie.h"
#include "./Headers.h"
#include "../Externals.h"

#define PRIME 31
/**
 * @brief Initializes a Reader_Writer_Lock Object
 * @param None
 * @return a pointer to Reader_Writer_Lock_Object
 * @note the lock maintains a service queue to prevent starvation
 */
Reader_Writer_Lock *RW_Lock_Init()
{
    Reader_Writer_Lock *Lock = (Reader_Writer_Lock *)malloc(sizeof(Reader_Writer_Lock));
    Lock->Reader_Count = 0;
    pthread_mutex_init(&Lock->Service_Q_Lock, NULL);
    pthread_mutex_init(&Lock->Read_Init_Lock, NULL);
    pthread_mutex_init(&Lock->Write_Lock, NULL);

    return Lock;
}
// Reader Accquire Lock
void Read_Lock(Reader_Writer_Lock *Lock)
{
    pthread_mutex_lock(&Lock->Service_Q_Lock);
    pthread_mutex_lock(&Lock->Read_Init_Lock);
    Lock->Reader_Count++;
    if (Lock->Reader_Count == 1)
    {
        pthread_mutex_lock(&Lock->Write_Lock);
    }
    pthread_mutex_unlock(&Lock->Read_Init_Lock);
    pthread_mutex_unlock(&Lock->Service_Q_Lock);
}
// Reader Release Lock
void Read_Unlock(Reader_Writer_Lock *Lock)
{
    pthread_mutex_lock(&Lock->Read_Init_Lock);
    Lock->Reader_Count--;
    if (Lock->Reader_Count == 0)
    {
        pthread_mutex_unlock(&Lock->Write_Lock);
    }
    pthread_mutex_unlock(&Lock->Read_Init_Lock);
}
// Writer Accquire Lock
void Write_Lock(Reader_Writer_Lock *Lock)
{
    pthread_mutex_lock(&Lock->Service_Q_Lock);
    pthread_mutex_lock(&Lock->Write_Lock);
    pthread_mutex_unlock(&Lock->Service_Q_Lock);
}
// Writer Release Lock
void Write_Unlock(Reader_Writer_Lock *Lock)
{
    pthread_mutex_unlock(&Lock->Write_Lock);
}

unsigned int hash(char *path_token)
{
    int hash = 0;
    for (int i = 0; i < strlen(path_token); i++)
    {
        // Use polynomial rolling hash function
        hash = (hash * PRIME + path_token[i]) % MAX_SUB_FILES;
    }
    return hash % MAX_SUB_FILES;
}

/**
 * @brief Recursively helper function to print the trie structure
 * @param file_trie the trie to be printed
 * @param buffer the buffer to be printed to
 * @param cur_dir the current directory path
 * @return 0 on success, -1 on failure
 * @note This function is called as a subroutine of trie_paths
 */
int trie_paths_helper(Trie *file_trie, char *buffer, char *cur_dir)
{
    if (file_trie == NULL)
    {
        return 0;
    }

    // Remove '/' if it is the last character
    if (cur_dir[strlen(cur_dir) - 1] == '/')
    {
        cur_dir[strlen(cur_dir) - 1] = '\0';
    }

    Read_Lock(file_trie->Lock);
    for (int i = 0; i < MAX_SUB_FILES; i++)
    {
        if (file_trie->children[i] != NULL)
        {
            char path[MAX_BUFFER_SIZE];
            snprintf(path, MAX_BUFFER_SIZE, "%s/%s", cur_dir, file_trie->children[i]->path_token);

            strncat(buffer, path, MAX_BUFFER_SIZE);
            strcat(buffer, "\n");
            Read_Unlock(file_trie->Lock);
            int status = trie_paths_helper(file_trie->children[i], buffer, path);
            Read_Lock(file_trie->Lock);
            if (CheckError(status, "trie_paths_helper: Error printing to buffer"))
            {
                fprintf(Log_File, "trie_paths_helper: Error printing to buffer [Time Stamp: %f]\n", GetCurrTime(Clock));
                return -1;
            }
        }
    }
    Read_Unlock(file_trie->Lock);
    return 0;
}

/**
 * @brief Initializes a Trie_Node Object to store lock corresponding to paths
 * @param None
 * @return a pointer to Trie_Node_Object
 * @note called as a subroutine of trie_insert
 */
Trie *trie_init()
{
    Trie *file_trie = (Trie *)malloc(sizeof(Trie));
    file_trie->path_token[0] = '\0';
    for (int i = 0; i < MAX_SUB_FILES; i++)
    {
        file_trie->children[i] = NULL;
    }
    file_trie->Lock = RW_Lock_Init();

    return file_trie;
}

/**
 * @brief Inserts a path into the trie
 * @param file_trie the trie to be inserted into
 * @param path the path to be inserted
 * @return 0 on success, -1 on failure
 */
int trie_insert(Trie *file_trie, char *path)
{
    char *path_token = strtok(path, "/");
    // Ignore the first token as it is the cwd
    path_token = strtok(NULL, "/");

    Trie *curr = file_trie;
    while (path_token != NULL)
    {
        int index = hash(path_token);
        Read_Lock(curr->Lock);
        if (curr->children[index] == NULL)
        {
            Read_Unlock(curr->Lock);
            Write_Lock(curr->Lock);
            curr->children[index] = trie_init();
            if (CheckNull(curr->children[index], "trie_insert: Error initializing trie node"))
            {
                Write_Unlock(curr->Lock);
                fprintf(Log_File, "trie_insert: Error initializing trie node [Time Stamp: %f]\n", GetCurrTime(Clock));
                return -1;
            }
            strncpy(curr->children[index]->path_token, path_token, TOKEN_SIZE);
            Write_Unlock(curr->Lock);
        }
        Read_Unlock(curr->Lock);
        curr = curr->children[index];
        path_token = strtok(NULL, "/");
    }
    return 0;
}

/**
 * @brief Gets the lock corresponding to a path in the trie
 * @param file_trie the trie to be searched
 * @param path the path to be inserted
 * @return a pointer to the lock corresponding to the path
 * @note returns NULL if path not found
 */
Reader_Writer_Lock *trie_get_path_lock(Trie *file_trie, char *path)
{
    char *path_token = strtok(path, "/");
    // Ignore the first token as it is the cwd
    path_token = strtok(NULL, "/");

    Trie *curr = file_trie;
    while (path_token != NULL)
    {
        int index = hash(path_token);
        if (curr->children[index] == NULL)
        {
            return NULL;
        }
        curr = curr->children[index];
        path_token = strtok(NULL, "/");
    }
    return curr->Lock;
}

/**
 * @brief Deletes a path from the trie
 * @param file_trie the trie to be deleted from
 * @param path the path to be deleted
 * @return 0 on success, -1 on failure
 */
int trie_delete(Trie *file_trie, char *path)
{
    char *path_token = strtok(path, "/");
    // Ignore the first token as it is the cwd
    path_token = strtok(NULL, "/");

    Trie *curr = file_trie;
    while (path_token != NULL)
    {
        int index = hash(path_token);
        Read_Lock(curr->Lock);
        if (curr->children[index] == NULL)
        {
            Read_Unlock(curr->Lock);
            return -1;
        }
        path_token = strtok(NULL, "/");
        if (path_token == NULL)
        {
            Trie *temp = curr->children[index];

            Read_Unlock(curr->Lock);
            Write_Lock(curr->Lock);
            curr->children[index] = NULL;
            Write_Unlock(curr->Lock);

            curr = temp;
        }
        else
        {
            Trie *temp = curr;
            curr = curr->children[index];
            Read_Unlock(temp->Lock);
        }
    }

    // Delete all children
    Write_Lock(curr->Lock);
    for (int i = 0; i < MAX_SUB_FILES; i++)
    {
        if (curr->children[i] != NULL)
        {
            int status = trie_destroy(curr->children[i]);
            if (CheckError(status, "trie_delete: Error deleting children"))
            {
                fprintf(Log_File, "trie_delete: Error deleting children [Time Stamp: %f]\n", GetCurrTime(Clock));
                return -1;
            }
            curr->children[i] = NULL;
        }
    }
    // dont need to unlock as the lock is destroyed
    // Write_Unlock(curr->Lock);

    // Delete the current node
    free(curr);
    return 0;
}

/**
 * @brief Recursively deletes a trie
 * @param file_trie the trie to be destroyed
 * @return 0 on success, -1 on failure
 * @note This function is called as a subroutine of trie_delete
 */
int trie_destroy(Trie *file_trie)
{
    Write_Lock(file_trie->Lock);
    // Delete all children
    for (int i = 0; i < MAX_SUB_FILES; i++)
    {
        if (file_trie->children[i] != NULL)
        {
            int status = trie_destroy(file_trie->children[i]);
            if (CheckError(status, "trie_destroy: Error deleting children"))
            {
                fprintf(Log_File, "trie_destroy: Error deleting children [Time Stamp: %f]\n", GetCurrTime(Clock));
                return -1;
            }
            file_trie->children[i] = NULL;
        }
    }

    // Dont need to unlock as the lock is destroyed
    // Write_Unlock(file_trie->Lock);

    // Delete the current node
    free(file_trie);
    return 0;
}


/**
 * @brief Renames a path in the trie
 * @param file_trie the trie to be renamed
 * @param old_path the old path to be renamed
 * @param new_token the new token to be renamed to
 * @return 0 on success, -1 on failure
 * @note the last token in the path is renamed
 */
int trie_rename(Trie *file_trie, char *old_path, char *new_token)
{
    char *path_token = strtok(old_path, "/");
    // Ignore the first token as it is the cwd
    path_token = strtok(NULL, "/");

    Trie *curr = file_trie;
    Trie *prev = NULL;
    int cur_index = -1;
    while (path_token != NULL)
    {
        int index = hash(path_token);
        Read_Lock(curr->Lock);
        if (curr->children[index] == NULL)
        {
            Read_Unlock(curr->Lock);
            return -1;
        }
        cur_index = index;
        prev = curr;
        curr = curr->children[index];
        path_token = strtok(NULL, "/");
    }

    Write_Lock(curr->Lock);
    int index = hash(new_token);
    if (prev->children[index] == NULL)
    {
        prev->children[cur_index] = NULL;
        prev->children[index] = curr;
        strncpy(prev->children[index]->path_token, new_token, TOKEN_SIZE);
        Write_Unlock(curr->Lock);
        return 0;
    }
    else
    {
        Write_Unlock(curr->Lock);
        return -1;
    }
    return 0;
}

/**
 * @brief Outputs the trie structure to a buffer
 * @param file_trie the trie to be printed
 * @param buffer the buffer to be printed to
 * @return 0 on success, -1 on failure
 */
int trie_print(Trie *file_trie, char *buffer, int level)
{
    if (file_trie == NULL)
    {
        return 0;
    }

    // Print the current node
    int status;
    for (int i = 0; i < level; i++)
    {
        if (i % 2 == 0)
            sprintf(buffer, "%s|", buffer);
        else
            sprintf(buffer, "%s  ", buffer);
    }
    Read_Lock(file_trie->Lock);
    status = sprintf(buffer, "%s|-%s\n", buffer, file_trie->path_token);
    if (CheckError(status, "trie_print: Error printing to buffer"))
    {
        Read_Unlock(file_trie->Lock);
        fprintf(Log_File, "trie_print: Error printing to buffer [Time Stamp: %f]\n", GetCurrTime(Clock));
        return -1;
    }

    for (int i = 0; i < MAX_SUB_FILES; i++)
    {
        if (file_trie->children[i] != NULL)
        {
            status = trie_print(file_trie->children[i], buffer, level + 1);
            if (CheckError(status, "trie_print: Error printing to buffer"))
            {
                fprintf(Log_File, "trie_print: Error printing to buffer [Time Stamp: %f]\n", GetCurrTime(Clock));
                return -1;
            }
        }
    }

    Read_Unlock(file_trie->Lock);
    return 0;
}

/**
 * @brief Outputs the paths in the trie to a buffer seperated by newlines
 * @param file_trie the trie to be printed
 * @param buffer the buffer to be printed to
 * @return 0 on success, -1 on failure
 */
int trie_paths(Trie *file_trie, char *buffer, char *root)
{
    if (file_trie == NULL)
    {
        return 0;
    }

    // Copy the root path to the buffer
    char root_path[MAX_BUFFER_SIZE];
    strncpy(root_path, root, MAX_BUFFER_SIZE);

    // traverse to the root
    Trie *curr = file_trie;
    char *path_token = strtok(root, "/");
    // Ignore the first token as it is the cwd
    path_token = strtok(NULL, "/");
    while (path_token != NULL)
    {
        int index = hash(path_token);
        Read_Lock(curr->Lock);
        if (CheckNull(curr->children[index], "trie_paths: Error traversing to root"))
        {
            Read_Unlock(curr->Lock);
            fprintf(Log_File, "trie_paths: Error traversing to root [Time Stamp: %f]\n", GetCurrTime(Clock));
            return -1;
        }

        Trie *temp = curr;
        curr = curr->children[index];
        Read_Unlock(temp->Lock);
        path_token = strtok(NULL, "/");
    }

    int status = trie_paths_helper(curr, buffer, root_path);
    if (CheckError(status, "trie_paths: Error printing to buffer"))
    {
        fprintf(Log_File, "trie_paths: Error printing to buffer [Time Stamp: %f]\n", GetCurrTime(Clock));
        return -1;
    }
    return 0;
}

/**
 * @brief Searches for a path in the trie
 * @param file_trie the trie to be searched
 * @param path the path to be searched
 * @return 1 if found, 0 if not found
 * @note modifies the path string provided
 */
int trie_search(Trie *file_trie, char *path)
{
    char *path_token = strtok(path, "/");
    // Ignore the first token as it is the cwd
    path_token = strtok(NULL, "/");

    Trie *curr = file_trie;
    while (path_token != NULL)
    {
        int index = hash(path_token);
        Read_Lock(curr->Lock);
        if (curr->children[index] == NULL)
        {
            Read_Unlock(curr->Lock);
            return 0;
        }
        Trie *temp = curr;
        curr = curr->children[index];
        Read_Unlock(temp->Lock);
        path_token = strtok(NULL, "/");
    }
    return 1;
}