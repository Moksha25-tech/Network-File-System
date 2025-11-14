#ifndef __TRIE_H__
#define __TRIE_H__

#include "Headers.h"

#define MAX_CHILDREN 512 // specifies the maximum nummber of contents in a directory (Keep High for good hash performance)


typedef struct TrieNode {
    char path_token[MAX_PATH_LEN];
    void* Server_Handle;
    struct TrieNode* children[MAX_CHILDREN];
}TrieNode;

// TrieNode* getNode(); // returns a new node
// int Hash(char* path_token); // returns the index of the child in the children array else -1
TrieNode* Init_Trie(); // returns the root node of the empty trie
int Insert_Path(TrieNode* root,char* path, void* Server_Handle); // inserts the path in the trie
void* Get_Server(TrieNode* root, char* path); // returns the server handle of the path
int Delete_Path(TrieNode* root, char* path); // deletes the path from the trie
int Delete_Trie(TrieNode* root); // deletes the trie
// int Recursive_Delete(TrieNode* root); // deletes the trie recursively

void Print_Trie(TrieNode* root, int lvl); // prints the trie
int Get_Directory_Tree(TrieNode* root, char* path, char* buffer); // Populates the buffer with the directory tree
// char* Get_Directory_Tree_Full(TrieNode* root, char* cur_dir, int lvl); // returns a string with the full tree path

#endif