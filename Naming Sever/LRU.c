#include "LRU.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


int cacheSize(LRUCache *cache)
{
    int size = 0;
    Node *current = cache->head;
    while (current != NULL)
    {
        size++;
        current = current->next;
    }
    return size;
}

int hashFunction(const char *key)
{
    // Simple hash function for illustration purposes
    // jenkins_hash
    unsigned long long hash = 0;
    while (*key)
    {
        hash += (unsigned long long)(*key++);
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash % CACHE_SIZE;
}

/**
 * @brief Initializes the Cache
 * @return: The cache object 
*/
LRUCache *createCache()
{
    LRUCache *cache = (LRUCache *)malloc(sizeof(LRUCache));
    cache->head = NULL;
    cache->tail = NULL;
    for (int i = 0; i < CACHE_SIZE; i++)
    {
        cache->hashmap[i] = NULL;
    }
    return cache;
}

Node *createNode(const char *key, void *value)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    strcpy(newNode->key, key);
    newNode->value = value;
    newNode->next = NULL;
    newNode->prev = NULL;
    return newNode;
}

void removeFromList(LRUCache *cache, Node *node)
{
    if (node->prev != NULL)
    {
        node->prev->next = node->next;
    }
    else
    {
        cache->head = node->next;
    }

    if (node->next != NULL)
    {
        node->next->prev = node->prev;
    }
    else
    {
        cache->tail = node->prev;
    }
}

void moveToHead(LRUCache *cache, Node *node)
{
    removeFromList(cache, node);

    node->next = cache->head;
    node->prev = NULL;

    if (cache->head != NULL)
    {
        cache->head->prev = node;
    }

    cache->head = node;

    if (cache->tail == NULL)
    {
        cache->tail = node;
    }
}

/**
 * @brief Adds a key-value pair to the cache
 * @param cache: The cache object
 * @param key: The key
 * @param value: The value
 * @return: void
 * @note: If the key already exists, the value is updated and the node is moved to the head
*/
void put(LRUCache *cache, const char *key, void *value)
{
    int index = hashFunction(key) % CACHE_SIZE;

    if (cache->hashmap[index] != NULL)
    {
        // Key already exists, update value and move to the head
        Node *node = cache->hashmap[index];
        node->value = value;
        moveToHead(cache, node);
    }
    else
    {
        // Key doesn't exist, create a new node and add to the head
        Node *newNode = createNode(key, value);
        cache->hashmap[index] = newNode;

        if (cache->head == NULL)
        {
            cache->head = newNode;
            cache->tail = newNode;
        }
        else
        {
            newNode->next = cache->head;
            cache->head->prev = newNode;
            cache->head = newNode;
        }

        // If the cache size exceeds the limit, remove the least recently used node
        if (cacheSize(cache) > CACHE_SIZE)
        {
            Node *tail = cache->tail;
            cache->tail = tail->prev;
            removeFromList(cache, tail);
            int removeIndex = hashFunction(tail->key) % CACHE_SIZE;
            cache->hashmap[removeIndex] = NULL;
            free(tail);
        }
    }
}

/**
 * @brief Gets the value for a given key
 * @param cache: The cache object
 * @param key: The key
 * @return: The value on success, NULL on failure
 * @note: Moves the accessed node to the head
*/
void *get(LRUCache *cache, const char *key)
{
    int index = hashFunction(key) % CACHE_SIZE;
    Node *node = cache->hashmap[index];

    if (node != NULL)
    {
        // Move the accessed node to the head
        moveToHead(cache, node);
        return node->value;
    }
    else
    {
        return NULL; // Key not found
    }
}

/**
 * @brief prints the cache
 * @param cache: The cache object
*/
void printCache(LRUCache *cache)
{
    Node *current = cache->head;
    while (current != NULL)
    {
        printf("(%s, %p) ", current->key, current->value);
        current = current->next;
    }
    printf("\n");
}

/**
 * @brief Deallocates the cache
 * @param cache: The cache object
 * @note: Removes all the nodes from the cache and frees the cache object
*/
void freeCache(LRUCache *cache)
{
    Node *current = cache->head;
    while (current != NULL)
    {
        Node *temp = current;
        current = current->next;
        free(temp);
    }
    free(cache);
}

/**
 * @brief Flushes the cache
 * @param cache: The cache object
 * @note: Removes all the nodes from the cache
*/
void flushCache(LRUCache* cache)
{
    memset(cache, 0, sizeof(LRUCache));
}