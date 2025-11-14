#include "Trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// local helper functions
TrieNode *getNode() // returns a new node
{
    TrieNode *node = (TrieNode *)calloc(1, sizeof(TrieNode));
    return node;
}

int Hash(char *path_token) // returns the index of the child in the children array with given token
{
    if (path_token == NULL)
        return -1;

    // djb2 algorithm
    unsigned long hash = 5381;
    int c;
    while (c = *path_token++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash % MAX_CHILDREN;
}
int Recursive_Delete(TrieNode *root) // deletes the subtree for the given node
{
    if (root == NULL)
        return -1;
    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (root->children[i] != NULL)
        {
            Recursive_Delete(root->children[i]);
            root->children[i] = NULL;
        }
    }
    free(root);
    return 0;
}
int Get_Directory_Tree_Full(TrieNode *root, char *buffer, int lvl) // returns a string with the full tree path
{
    if (root == NULL)
        return -1;
    for (int i = 0; i < lvl; i++)
    {
        if (i%2 == 0 || i == 0)
            strncat(buffer,"|",MAX_BUFFER_SIZE - strlen(buffer) - 1);
        else
            strncat(buffer," ",MAX_BUFFER_SIZE - strlen(buffer) - 1);    }

    strncat(buffer, "|-", MAX_BUFFER_SIZE - strlen(buffer) - 1);
    strncat(buffer, root->path_token, MAX_BUFFER_SIZE - strlen(buffer) - 1);
    strncat(buffer, "\n", MAX_BUFFER_SIZE - strlen(buffer) - 1);

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (root->children[i] != NULL)
        {
            int err = Get_Directory_Tree_Full(root->children[i], buffer, lvl + 1);
            if (err < 0)
                return -1;
        }
    }
    return 0;
}

// global functions
/**
 * @brief Initializes the trie
 * @return: The root node of the empty trie
 */
TrieNode *Init_Trie() // returns the root node of the empty trie
{
    TrieNode *root = getNode();
    return root;
}
/**
 * @brief Inserts the path in the trie
 * @param root: The root node of the trie
 * @param path: The path to be inserted
 * @param Server_Handle: The server handle of the path
 * @return: 0 on success, -1 on failure
 */
int Insert_Path(TrieNode *root, char *path, void *Server_Handle)
{
    if (root == NULL || path == NULL || Server_Handle == NULL)
        return -1;

    TrieNode *curr = root;
    char *path_token = strtok(path, "/");
    // Ignore the first token as it is CWD for Storage Server
    path_token = strtok(NULL, "/");

    while (path_token != NULL)
    {
        int index = Hash(path_token);

        // Check if the current node has a valid path_token
        // if (curr->Server_Handle == NULL) {
        //     strcpy(curr->path_token, path_token);
        //     curr->Server_Handle = Server_Handle;
        // }

        if (curr->children[index] == NULL)
        {
            curr->children[index] = getNode();
            if (curr->children[index] == NULL)
                return -1;
            strcpy(curr->children[index]->path_token, path_token);
            curr->children[index]->Server_Handle = Server_Handle;
        }

        curr = curr->children[index];
        path_token = strtok(NULL, "/");
    }

    // Set the final node's Server_Handle
    curr->Server_Handle = Server_Handle;

    return 0;
}
/**
 * @brief Returns the server handle of the path
 * @param root: The root node of the trie
 * @param path: The path for which the server handle is to be returned
 * @return: The server handle of the path
 * @note: Returns NULL if the path is not present in the trie
 */
void *Get_Server(TrieNode *root, char *path) // returns the server handle of the path
{
    if (root == NULL || path == NULL)
        return NULL;
    TrieNode *curr = root;
    char *path_cpy = (char *)calloc(strlen(path) + 1, sizeof(char));
    strcpy(path_cpy, path);
    char *path_token = strtok(path_cpy, "/");
    path_token = strtok(NULL, "/");
    while (path_token != NULL)
    {
        int index = Hash(path_token);
        if (curr->children[index] == NULL)
            return NULL;
        curr = curr->children[index];
        path_token = strtok(NULL, "/");
    }
    free(path_cpy);
    return curr->Server_Handle;
}
/**
 * @brief Deletes the path from the trie
 * @param root: The root node of the trie
 * @param path: The path to be deleted
 * @return: 0 on success, -1 on failure
 * @note: Deletes the subtree for the given path
 */
int Delete_Path(TrieNode *root, char *path) // deletes the path from the trie
{
    if (root == NULL || path == NULL)
        return -1;
    TrieNode *curr = root;
    TrieNode *prev = NULL;
    char *path_token = strtok(path, "/");
    int index;
    while (path_token != NULL)
    {
        index = Hash(path_token);
        if (curr->children[index] == NULL)
            return -1;
        prev = curr;
        curr = curr->children[index];
        path_token = strtok(NULL, "/");
    }

    // Delete the subtree for the given path
    prev->children[index] = NULL;
    return Recursive_Delete(curr);
}
/**
 * @brief Deletes the trie
 * @param root: The root node of the trie
 * @return: 0 on success, -1 on failure
 * @note: Deletes the trie recursively
 */
int Delete_Trie(TrieNode *root) // deletes the trie
{
    if (root == NULL)
        return -1;
    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (root->children[i] != NULL)
        {
            Delete_Trie(root->children[i]);
        }
    }
    free(root);
    return 0;
}

// helper global functions
/**
 * @brief Prints the trie
 * @param root: The root node of the trie
 * @param lvl: The level of the node in the trie
 * @note: Prints the trie recursively
 */
void Print_Trie(TrieNode *root, int lvl) // prints the trie
{
    if (root == NULL)
        return;
    for (int i = 0; i < lvl; i++)
    {
        if (i%2 == 0)
            printf("|");
        else
            printf(" ");
    }
    unsigned long server_id = root->Server_Handle == NULL ? -1 : ((SERVER_HANDLE_STRUCT*)root->Server_Handle)->ServerID;
    printf("|-%s (Server ID: %lu)\n", root->path_token, server_id);
    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (root->children[i] != NULL)
        {
            Print_Trie(root->children[i], lvl + 1);
        }
    }
    return;
}
/**
 * @brief Populates the buffer with subtree path for a given path
 * @param root: The root node of the trie
 * @param path: The path for which the subtree path is to be returned
 * @param buffer: The buffer to be populated
 * @return: The buffer with subtree path for a given path
 * @note: Returns 0 on success, -1 if the path is not present in the trie and -2 on error
 * @note: The buffer should be allocated before calling this function (assumes buffer is big enough)
 */
int Get_Directory_Tree(TrieNode *root, char *path, char* buffer) // fills the buffer with subtree path for a given path
{
    if (root == NULL || path == NULL)
        return -2;
    // itterate to the node for the given path
    TrieNode *curr = root;

    char *path_cpy = (char *) malloc(strlen(path) + 1);
    memset(path_cpy, 0, strlen(path) + 1);

    strcpy(path_cpy, path);
    char *path_token = strtok(path_cpy, "/");
    path_token = strtok(NULL, "/");
    while (path_token != NULL)
    {
        int index = Hash(path_token);
        if (curr->children[index] == NULL)
        {
            free(path_cpy);
            strcpy(buffer, "Invalid Path");
            return -1;
        }
        curr = curr->children[index];
        path_token = strtok(NULL, "/");
    }
    free(path_cpy);

    // Recursively get the subtree path
    // char *subtree_path = (char *)calloc(MAX_BUFFER_SIZE, sizeof(char));

    if(Get_Directory_Tree_Full(curr,buffer , 0) < 0)
    {
        strcpy(buffer, "Error in getting subtree directory");
        return -2;
    }
    return 0; 
}
