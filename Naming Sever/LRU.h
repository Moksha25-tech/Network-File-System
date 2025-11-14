#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include "Headers.h"

// Define the maximum size of the cache
#define CACHE_SIZE 19

typedef struct Node {
    char key[MAX_PATH_LEN];
    void* value;
    struct Node* next;
    struct Node* prev;
} Node;

typedef struct LRUCache {
    Node* head;
    Node* tail;
    Node* hashmap[CACHE_SIZE];
} LRUCache;

//Global
LRUCache* createCache(); 
void put(LRUCache* cache, const char* key, void* value); 
void* get(LRUCache* cache, const char* key); 
void freeCache(LRUCache* cache);
void printCache(LRUCache* cache); 
void flushCache(LRUCache* cache);


//Local Helpers
/*
Node* createNode(const char* key, void* value);
void removeFromList(LRUCache* cache, Node* node);
void moveToHead(LRUCache* cache, Node* node);
int cacheSize(LRUCache* cache);
int hashFunction(const char* key);
*/

#endif /* LRU_CACHE_H */
