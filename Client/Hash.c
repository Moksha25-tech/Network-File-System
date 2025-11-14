#include "Hash.h"
#include <stdlib.h>

#define PRIME 37

/**
 * @brief Hashes a string to an integer (polyhash)
*/
int hash(char* key, int capacity)
{
    int hash = 0;
    int i = 0;
    while (key[i] != '\0')
    {
        hash += key[i] + hash * PRIME;
        hash %= capacity;
        i++;
    }
    return hash % capacity;
}
/**
 * @brief Creates a hash table
 * @param capacity The capacity of the hash table
 * @return A pointer to the hash table
*/
HashTable* createHashTable(int capacity)
{
    HashTable* table = (HashTable*)malloc(sizeof(HashTable));
    table->capacity = capacity;
    table->size = 0;
    table->functionArr = (functionPointer*)malloc(sizeof(functionPointer) * capacity);

    for (int i = 0; i < capacity; i++)
    {
        table->functionArr[i] = NULL;
    }

    return table;
}
/**
 * @brief Destroys a hash table
 * @param table The hash table to destroy
 * @return void
 * @note This function does not free the function pointers in the hash table
*/
void destroyHashTable(HashTable* table)
{
    /*
    for (int i = 0; i < table->capacity; i++)
    {
        if (table->functionArr[i] != NULL)
        {
            free(table->functionArr[i]);
        }
    }
    */
    free(table->functionArr);
    free(table);
    return;
}
/**
 * @brief Inserts a function pointer into the hash table
 * @param table The hash table to insert into
 * @param function The function pointer to insert
 * @param key The key to insert the function pointer at
 * @return void
*/
void insert(HashTable* table, functionPointer function, char* key)
{
    int index = hash(key, table->capacity);
    table->functionArr[index] = function;
    table->size++;
    return;
}
/**
 * @brief Looks up a function pointer in the hash table
 * @param table The hash table to look up in
 * @param key The key to look up
 * @return The function pointer at the key
 * @note If the key is not found, this function returns NULL
*/
functionPointer lookup(HashTable* table, char* key)
{
    int index = hash(key, table->capacity);
    return table->functionArr[index];
}
