#ifndef __TRIE_H__
#define __TRIE_H__

#include <pthread.h>

#define MAX_SUB_FILES 512 // Max number of sub files in a directory(High for good hash distribution)
#define TOKEN_SIZE 32 // Max size of a path token

// Reader/Writer lock struct
typedef struct Reader_Writer_Lock
{
    pthread_mutex_t Service_Q_Lock;
    pthread_mutex_t Read_Init_Lock;
    pthread_mutex_t Write_Lock;

    int Reader_Count;
}Reader_Writer_Lock;

Reader_Writer_Lock *RW_Lock_Init();
void Read_Lock(Reader_Writer_Lock *Lock);
void Read_Unlock(Reader_Writer_Lock *Lock);
void Write_Lock(Reader_Writer_Lock *Lock);
void Write_Unlock(Reader_Writer_Lock *Lock);

// Trie node struct
typedef struct Trie_Node
{
    char path_token[TOKEN_SIZE];
    Reader_Writer_Lock* Lock;
    struct Trie_Node *children[MAX_SUB_FILES];
}Trie_Node;

typedef Trie_Node Trie;

// Function prototypes
Trie* trie_init(); // Initialize the trie on startup in the cwd for all paths in cwd
int trie_insert(Trie* file_trie, char* path); // Insert a path into the trie
Reader_Writer_Lock* trie_get_path_lock(Trie* file_trie, char* path); // Get correspomding lock for a path in trie
int trie_delete(Trie* file_trie, char *path); // Delete a path from the trie (deletes all children path)
int trie_destroy(Trie* file_trie); // Destroy the trie on shutdown
int trie_rename(Trie* file_trie, char* old_path, char* new_token); // Rename a path in the trie

int trie_search(Trie* file_trie, char* path); // Search for a path in the trie
int trie_print(Trie* file_trie, char* buffer, int level); // Print the trie
int trie_paths(Trie* file_trie, char* buffer, char* root); // Get all paths in the trie under root-path

#endif // __TRIE_H__